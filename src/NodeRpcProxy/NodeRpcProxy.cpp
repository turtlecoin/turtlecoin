// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.


#include "NodeRpcProxy.h"
#include "NodeErrors.h"

#include <atomic>
#include <ctime>
#include <system_error>
#include <thread>

#include <HTTP/HttpRequest.h>
#include <HTTP/HttpResponse.h>
#include <System/ContextGroup.h>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/EventLock.h>
#include <System/Timer.h>
#include <CryptoNoteCore/TransactionApi.h>

#include "Common/StringTools.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/CryptoNoteTools.h"

#include <Logging/DummyLogger.h>

#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "Rpc/HttpClient.h"
#include "Rpc/JsonRpc.h"

#ifndef AUTO_VAL_INIT
#define AUTO_VAL_INIT(n) boost::value_initialized<decltype(n)>()
#endif

using namespace Crypto;
using namespace Common;
using namespace Logging;
using namespace System;

namespace CryptoNote {

namespace {

std::error_code interpretResponseStatus(const std::string& status) {
  if (CORE_RPC_STATUS_BUSY == status) {
    return make_error_code(NodeError::NODE_BUSY);
  } else if (CORE_RPC_STATUS_OK != status) {
    return make_error_code(NodeError::INTERNAL_NODE_ERROR);
  }
  return std::error_code();
}

}

NodeRpcProxy::NodeRpcProxy(const std::string& nodeHost, unsigned short nodePort, unsigned int initTimeout, std::shared_ptr<Logging::ILogger> logger) :
  m_logger(logger, "NodeRpcProxy"),
  m_rpcTimeout(10000),
  m_pullInterval(5000),
  m_nodeHost(nodeHost),
  m_nodePort(nodePort),
  m_initTimeout(initTimeout),
  m_connected(true),
  m_peerCount(0),
  m_networkHeight(0),
  m_nodeHeight(0)
{
  resetInternalState();
}

NodeRpcProxy::NodeRpcProxy(const std::string& nodeHost, unsigned short nodePort, unsigned int initTimeout) :
  m_logger(std::make_shared<Logging::DummyLogger>(), "NodeRpcProxy"),
  m_rpcTimeout(10000),
  m_pullInterval(5000),
  m_nodeHost(nodeHost),
  m_nodePort(nodePort),
  m_initTimeout(initTimeout),
  m_connected(true),
  m_peerCount(0),
  m_networkHeight(0),
  m_nodeHeight(0)
{
  resetInternalState();
}

NodeRpcProxy::~NodeRpcProxy() {
  try {
    shutdown();
  } catch (std::exception&) {
  }
}

void NodeRpcProxy::resetInternalState() {
  m_stop = false;
  m_peerCount.store(0, std::memory_order_relaxed);
  m_networkHeight.store(0, std::memory_order_relaxed);
  lastLocalBlockHeaderInfo.index = 0;
  lastLocalBlockHeaderInfo.majorVersion = 0;
  lastLocalBlockHeaderInfo.minorVersion = 0;
  lastLocalBlockHeaderInfo.timestamp = 0;
  lastLocalBlockHeaderInfo.hash = CryptoNote::NULL_HASH;
  lastLocalBlockHeaderInfo.prevHash = CryptoNote::NULL_HASH;
  lastLocalBlockHeaderInfo.nonce = 0;
  lastLocalBlockHeaderInfo.isAlternative = false;
  lastLocalBlockHeaderInfo.depth = 0;
  lastLocalBlockHeaderInfo.difficulty = 0;
  lastLocalBlockHeaderInfo.reward = 0;
  m_knownTxs.clear();
}

void NodeRpcProxy::init(const INode::Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_state != STATE_NOT_INITIALIZED) {
    callback(make_error_code(NodeError::ALREADY_INITIALIZED));
    return;
  }

  m_state = STATE_INITIALIZING;
  resetInternalState();
  m_workerThread = std::thread([this, callback] {
    workerThread(callback);
  });
}

bool NodeRpcProxy::shutdown() {
  if (m_workerThread.joinable()) {
    m_workerThread.detach();
  }

  m_state = STATE_NOT_INITIALIZED;

  return true;
}

void NodeRpcProxy::workerThread(const INode::Callback& initialized_callback) {
  try {
    Dispatcher dispatcher;
    m_dispatcher = &dispatcher;
    ContextGroup contextGroup(dispatcher);
    m_context_group = &contextGroup;
    HttpClient httpClient(dispatcher, m_nodeHost, m_nodePort);
    m_httpClient = &httpClient;
    Event httpEvent(dispatcher);
    m_httpEvent = &httpEvent;
    m_httpEvent->set();

    {
      std::lock_guard<std::mutex> lock(m_mutex);
      assert(m_state == STATE_INITIALIZING);
      m_state = STATE_INITIALIZED;
      m_cv_initialized.notify_all();
    }

    std::future<void> init = std::async(std::launch::async, [this]
    {
        updateNodeStatus();
    });

    /* Init succeeded */
    if (init.wait_for(std::chrono::seconds(m_initTimeout)) == std::future_status::ready) {
        initialized_callback(std::error_code());
    /* Timed out initting */
    } else {
        initialized_callback(make_error_code(NodeError::TIMEOUT));
    }

    /* Wait for the init to actually complete so we're not doing stuff twice */
    init.get();

    getFeeInfo();

    contextGroup.spawn([this]() {
      Timer pullTimer(*m_dispatcher);
      while (!m_stop) {
        updateNodeStatus();
        if (!m_stop) {
          pullTimer.sleep(std::chrono::milliseconds(m_pullInterval));
        }
      }
    });

    contextGroup.wait();
    // Make sure all remote spawns are executed
    m_dispatcher->yield();
  } catch (std::exception&) {
  }

  m_dispatcher = nullptr;
  m_context_group = nullptr;
  m_httpClient = nullptr;
  m_httpEvent = nullptr;
  m_connected = false;
  m_rpcProxyObserverManager.notify(&INodeRpcProxyObserver::connectionStatusUpdated, m_connected);
}

void NodeRpcProxy::updateNodeStatus() {
  bool updateBlockchain = true;
  while (updateBlockchain) {
    updateBlockchainStatus();
    updateBlockchain = !updatePoolStatus();
  }
}

bool NodeRpcProxy::updatePoolStatus() {
  std::vector<Crypto::Hash> knownTxs = getKnownTxsVector();
  Crypto::Hash tailBlock = lastLocalBlockHeaderInfo.hash;

  bool isBcActual = false;
  std::vector<std::unique_ptr<ITransactionReader>> addedTxs;
  std::vector<Crypto::Hash> deletedTxsIds;

  std::error_code ec = doGetPoolSymmetricDifference(std::move(knownTxs), tailBlock, isBcActual, addedTxs, deletedTxsIds);
  if (ec) {
    return true;
  }

  if (!isBcActual) {
    return false;
  }

  if (!addedTxs.empty() || !deletedTxsIds.empty()) {
    updatePoolState(addedTxs, deletedTxsIds);
    m_observerManager.notify(&INodeObserver::poolChanged);
  }

  return true;
}

void NodeRpcProxy::updateBlockchainStatus() {
  CryptoNote::COMMAND_RPC_GET_LAST_BLOCK_HEADER::request req = AUTO_VAL_INIT(req);
  CryptoNote::COMMAND_RPC_GET_LAST_BLOCK_HEADER::response rsp = AUTO_VAL_INIT(rsp);

  std::error_code ec = jsonRpcCommand("getlastblockheader", req, rsp);

  if (!ec) {
    Crypto::Hash blockHash;
    Crypto::Hash prevBlockHash;
    if (!parse_hash256(rsp.block_header.hash, blockHash) || !parse_hash256(rsp.block_header.prev_hash, prevBlockHash)) {
      return;
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    uint32_t blockIndex = rsp.block_header.height;
    if (blockHash != lastLocalBlockHeaderInfo.hash) {
      lastLocalBlockHeaderInfo.index = blockIndex;
      lastLocalBlockHeaderInfo.majorVersion = rsp.block_header.major_version;
      lastLocalBlockHeaderInfo.minorVersion = rsp.block_header.minor_version;
      lastLocalBlockHeaderInfo.timestamp = rsp.block_header.timestamp;
      lastLocalBlockHeaderInfo.hash = blockHash;
      lastLocalBlockHeaderInfo.prevHash = prevBlockHash;
      lastLocalBlockHeaderInfo.nonce = rsp.block_header.nonce;
      lastLocalBlockHeaderInfo.isAlternative = rsp.block_header.orphan_status;
      lastLocalBlockHeaderInfo.depth = rsp.block_header.depth;
      lastLocalBlockHeaderInfo.difficulty = rsp.block_header.difficulty;
      lastLocalBlockHeaderInfo.reward = rsp.block_header.reward;
      lock.unlock();
      m_observerManager.notify(&INodeObserver::localBlockchainUpdated, blockIndex);
    }
  }

  CryptoNote::COMMAND_RPC_GET_INFO::request getInfoReq = AUTO_VAL_INIT(getInfoReq);
  CryptoNote::COMMAND_RPC_GET_INFO::response getInfoResp = AUTO_VAL_INIT(getInfoResp);

  ec = jsonCommand("/getinfo", getInfoReq, getInfoResp);
  if (!ec) {
    //a quirk to let wallets work with previous versions daemons.
    //Previous daemons didn't have the 'last_known_block_index' parameter in RPC so it may have zero value.
    std::unique_lock<std::mutex> lock(m_mutex);
    auto lastKnownBlockIndex = std::max(getInfoResp.last_known_block_index, lastLocalBlockHeaderInfo.index);
    lock.unlock();
    if (m_networkHeight.load(std::memory_order_relaxed) != lastKnownBlockIndex) {
      m_networkHeight.store(lastKnownBlockIndex, std::memory_order_relaxed);
      m_observerManager.notify(&INodeObserver::lastKnownBlockHeightUpdated, m_networkHeight.load(std::memory_order_relaxed));
    }

    m_nodeHeight.store(getInfoResp.height, std::memory_order_relaxed);

    updatePeerCount(getInfoResp.incoming_connections_count + getInfoResp.outgoing_connections_count);
  }

  if (m_connected != m_httpClient->isConnected()) {
    m_connected = m_httpClient->isConnected();
    m_rpcProxyObserverManager.notify(&INodeRpcProxyObserver::connectionStatusUpdated, m_connected);
  }
}

void NodeRpcProxy::updatePeerCount(size_t peerCount) {
  if (peerCount != m_peerCount) {
    m_peerCount = peerCount;
    m_observerManager.notify(&INodeObserver::peerCountUpdated, m_peerCount.load(std::memory_order_relaxed));
  }
}

void NodeRpcProxy::updatePoolState(const std::vector<std::unique_ptr<ITransactionReader>>& addedTxs, const std::vector<Crypto::Hash>& deletedTxsIds) {
  for (const auto& hash : deletedTxsIds) {
    m_knownTxs.erase(hash);
  }

  for (const auto& tx : addedTxs) {
    Hash hash = tx->getTransactionHash();
    m_knownTxs.emplace(std::move(hash));
  }
}

void NodeRpcProxy::getFeeInfo() {
  CryptoNote::COMMAND_RPC_GET_FEE_ADDRESS::request ireq = AUTO_VAL_INIT(ireq);
  CryptoNote::COMMAND_RPC_GET_FEE_ADDRESS::response iresp = AUTO_VAL_INIT(iresp);

  std::error_code ec = jsonCommand("/feeinfo", ireq, iresp);

  if (ec || iresp.status != CORE_RPC_STATUS_OK) {
    return;
  }
  m_fee_address = iresp.address;
  m_fee_amount = iresp.amount;

  return;
}

std::string NodeRpcProxy::feeAddress() {
  return m_fee_address;
}

uint32_t NodeRpcProxy::feeAmount() {
  return m_fee_amount;
}

std::vector<Crypto::Hash> NodeRpcProxy::getKnownTxsVector() const {
  return std::vector<Crypto::Hash>(m_knownTxs.begin(), m_knownTxs.end());
}

bool NodeRpcProxy::addObserver(INodeObserver* observer) {
  return m_observerManager.add(observer);
}

bool NodeRpcProxy::removeObserver(INodeObserver* observer) {
  return m_observerManager.remove(observer);
}

bool NodeRpcProxy::addObserver(CryptoNote::INodeRpcProxyObserver* observer) {
  return m_rpcProxyObserverManager.add(observer);
}

bool NodeRpcProxy::removeObserver(CryptoNote::INodeRpcProxyObserver* observer) {
  return m_rpcProxyObserverManager.remove(observer);
}

size_t NodeRpcProxy::getPeerCount() const {
  return m_peerCount.load(std::memory_order_relaxed);
}

uint32_t NodeRpcProxy::getLastLocalBlockHeight() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return lastLocalBlockHeaderInfo.index;
}

uint32_t NodeRpcProxy::getLastKnownBlockHeight() const {
  return m_networkHeight.load(std::memory_order_relaxed);
}

uint32_t NodeRpcProxy::getLocalBlockCount() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return lastLocalBlockHeaderInfo.index + 1;
}

uint32_t NodeRpcProxy::getKnownBlockCount() const {
  return m_networkHeight.load(std::memory_order_relaxed) + 1;
}

uint64_t NodeRpcProxy::getNodeHeight() const {
  return m_nodeHeight.load(std::memory_order_relaxed);
}

BlockHeaderInfo NodeRpcProxy::getLastLocalBlockHeaderInfo() const {
  std::lock_guard<std::mutex> lock(m_mutex);
  return lastLocalBlockHeaderInfo;
}

void NodeRpcProxy::getBlockHashesByTimestamps(uint64_t timestampBegin, size_t secondsCount, std::vector<Crypto::Hash>& blockHashes, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(NodeError::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetBlockHashesByTimestamps, this, timestampBegin, secondsCount, std::ref(blockHashes)),
                  callback);
}

void NodeRpcProxy::getTransactionHashesByPaymentId(const Crypto::Hash& paymentId, std::vector<Crypto::Hash>& transactionHashes, const INode::Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(NodeError::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetTransactionHashesByPaymentId, this, std::cref(paymentId), std::ref(transactionHashes)), callback);
}

std::error_code NodeRpcProxy::doGetBlockHashesByTimestamps(uint64_t timestampBegin, size_t secondsCount, std::vector<Crypto::Hash>& blockHashes) {
  COMMAND_RPC_GET_BLOCKS_HASHES_BY_TIMESTAMPS::request req = AUTO_VAL_INIT(req);
  COMMAND_RPC_GET_BLOCKS_HASHES_BY_TIMESTAMPS::response rsp = AUTO_VAL_INIT(rsp);

  req.timestampBegin = timestampBegin;
  req.secondsCount = secondsCount;

  std::error_code ec = jsonCommand("/get_blocks_hashes_by_timestamps", req, rsp);
  if (!ec) {
    blockHashes = std::move(rsp.blockHashes);
  }

  return ec;
}

void NodeRpcProxy::relayTransaction(const CryptoNote::Transaction& transaction, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(NodeError::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doRelayTransaction, this, transaction), callback);
}

void NodeRpcProxy::getRandomOutsByAmounts(std::vector<uint64_t>&& amounts, uint16_t outsCount,
                                          std::vector<RandomOuts>& outs,
                                          const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(NodeError::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetRandomOutsByAmounts, this, std::move(amounts), outsCount, std::ref(outs)),
    callback);
}

void NodeRpcProxy::getTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash,
                                                   std::vector<uint32_t>& outsGlobalIndices, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(NodeError::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetTransactionOutsGlobalIndices, this, transactionHash,
    std::ref(outsGlobalIndices)), callback);
}

void NodeRpcProxy::getGlobalIndexesForRange(
    const uint64_t startHeight,
    const uint64_t endHeight,
    std::unordered_map<Crypto::Hash, std::vector<uint64_t>> &indexes,
    const Callback &callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != STATE_INITIALIZED)
    {
        callback(make_error_code(NodeError::NOT_INITIALIZED));
        return;
    }

    scheduleRequest(std::bind(&NodeRpcProxy::doGetGlobalIndexesForRange, this, startHeight, endHeight, std::ref(indexes)), callback);
}

void NodeRpcProxy::getTransactionsStatus(
    const std::unordered_set<Crypto::Hash> transactionHashes,
    std::unordered_set<Crypto::Hash> &transactionsInPool,
    std::unordered_set<Crypto::Hash> &transactionsInBlock,
    std::unordered_set<Crypto::Hash> &transactionsUnknown,
    const Callback &callback)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state != STATE_INITIALIZED)
    {
        callback(make_error_code(NodeError::NOT_INITIALIZED));
        return;
    }

    scheduleRequest(
        std::bind(&NodeRpcProxy::doGetTransactionsStatus, this, transactionHashes,
                  std::ref(transactionsInPool), std::ref(transactionsInBlock),
                  std::ref(transactionsUnknown)),
        callback
    );
}

void NodeRpcProxy::queryBlocks(std::vector<Crypto::Hash>&& knownBlockIds, uint64_t timestamp, std::vector<BlockShortEntry>& newBlocks,
  uint32_t& startHeight, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(NodeError::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doQueryBlocksLite, this, std::move(knownBlockIds), timestamp,
          std::ref(newBlocks), std::ref(startHeight)), callback);
}

void NodeRpcProxy::getWalletSyncData(std::vector<Crypto::Hash>&& knownBlockIds, uint64_t startHeight, uint64_t startTimestamp, std::vector<WalletTypes::WalletBlockInfo>& newBlocks,
  const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(NodeError::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetWalletSyncData, this, std::move(knownBlockIds), startHeight, startTimestamp, std::ref(newBlocks)), callback);
}


void NodeRpcProxy::getPoolSymmetricDifference(std::vector<Crypto::Hash>&& knownPoolTxIds, Crypto::Hash knownBlockId, bool& isBcActual,
        std::vector<std::unique_ptr<ITransactionReader>>& newTxs, std::vector<Crypto::Hash>& deletedTxIds, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(NodeError::NOT_INITIALIZED));
    return;
  }

  scheduleRequest([this, knownPoolTxIds, knownBlockId, &isBcActual, &newTxs, &deletedTxIds] () mutable -> std::error_code {
    return this->doGetPoolSymmetricDifference(std::move(knownPoolTxIds), knownBlockId, isBcActual, newTxs, deletedTxIds); } , callback);
}

void NodeRpcProxy::getBlocks(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(NodeError::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetBlocksByHeight, this, std::cref(blockHeights), std::ref(blocks)), callback);
}

void NodeRpcProxy::getBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(NodeError::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetBlocksByHash, this, std::cref(blockHashes), std::ref(blocks)), callback);
}

void NodeRpcProxy::getBlock(const uint32_t blockHeight, BlockDetails &block, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(NodeError::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetBlock, this, blockHeight, std::ref(block)), callback);
}

void NodeRpcProxy::getTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(NodeError::NOT_INITIALIZED));
    return;
  }

  scheduleRequest(std::bind(&NodeRpcProxy::doGetTransactions, this, std::cref(transactionHashes), std::ref(transactions)), callback);
}

void NodeRpcProxy::isSynchronized(bool& syncStatus, const Callback& callback) {
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_state != STATE_INITIALIZED) {
    callback(make_error_code(NodeError::NOT_INITIALIZED));
    return;
  }

  // TODO NOT IMPLEMENTED
  callback(std::error_code());
}

std::error_code NodeRpcProxy::doRelayTransaction(const CryptoNote::Transaction& transaction) {
  COMMAND_RPC_SEND_RAW_TX::request req;
  COMMAND_RPC_SEND_RAW_TX::response rsp;
  req.tx_as_hex = toHex(toBinaryArray(transaction));
  m_logger(TRACE) << "NodeRpcProxy::doRelayTransaction, tx hex " << req.tx_as_hex;
  return jsonCommand("/sendrawtransaction", req, rsp);
}

std::error_code NodeRpcProxy::doGetRandomOutsByAmounts(std::vector<uint64_t>& amounts, uint16_t outsCount,
                                                       std::vector<RandomOuts>& outs) {
  COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request req = AUTO_VAL_INIT(req);
  COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response rsp = AUTO_VAL_INIT(rsp);
  req.amounts = std::move(amounts);
  req.outs_count = outsCount;

  m_logger(TRACE) << "Send getrandom_outs request";
  std::error_code ec = jsonCommand("/getrandom_outs", req, rsp);
  if (!ec) {
    m_logger(TRACE) << "getrandom_outs complete";
    outs = std::move(rsp.outs);
  } else {
    m_logger(TRACE) << "getrandom_outs failed: " << ec << ", " << ec.message();
  }

  return ec;
}

std::error_code NodeRpcProxy::doGetTransactionOutsGlobalIndices(const Crypto::Hash& transactionHash,
                                                                std::vector<uint32_t>& outsGlobalIndices) {
  CryptoNote::COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::request req = AUTO_VAL_INIT(req);
  CryptoNote::COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response rsp = AUTO_VAL_INIT(rsp);
  req.txid = transactionHash;

  m_logger(TRACE) << "Send get_o_indexes request, transaction " << req.txid;
  std::error_code ec = jsonCommand("/get_o_indexes", req, rsp);
  if (!ec) {
    m_logger(TRACE) << "get_o_indexes complete";
    outsGlobalIndices.clear();
    for (auto idx : rsp.o_indexes) {
      outsGlobalIndices.push_back(static_cast<uint32_t>(idx));
    }
  } else {
    m_logger(TRACE) << "get_o_indexes failed: " << ec << ", " << ec.message();
  }

  return ec;
}

std::error_code NodeRpcProxy::doGetGlobalIndexesForRange(
    const uint64_t startHeight,
    const uint64_t endHeight,
    std::unordered_map<Crypto::Hash, std::vector<uint64_t>> &indexes)
{
    CryptoNote::COMMAND_RPC_GET_GLOBAL_INDEXES_FOR_RANGE::request req = AUTO_VAL_INIT(req);
    CryptoNote::COMMAND_RPC_GET_GLOBAL_INDEXES_FOR_RANGE::response rsp = AUTO_VAL_INIT(rsp);

    req.startHeight = startHeight;
    req.endHeight = endHeight;

    m_logger(TRACE) << "Send get_global_indexes_for_range request";

    std::error_code ec = jsonCommand("/get_global_indexes_for_range", req, rsp);

    if (!ec)
    {
        m_logger(TRACE) << "get_global_indexes_for_range complete";
    }
    else
    {
        m_logger(TRACE) << "get_global_indexes_for_range failed: " << ec << ", " << ec.message();
    }

    indexes.insert(rsp.indexes.begin(), rsp.indexes.end());

    return ec;
}

std::error_code NodeRpcProxy::doGetTransactionsStatus(
    const std::unordered_set<Crypto::Hash> transactionHashes,
    std::unordered_set<Crypto::Hash> &transactionsInPool,
    std::unordered_set<Crypto::Hash> &transactionsInBlock,
    std::unordered_set<Crypto::Hash> &transactionsUnknown)
{
    CryptoNote::COMMAND_RPC_GET_TRANSACTIONS_STATUS::request req = AUTO_VAL_INIT(req);
    CryptoNote::COMMAND_RPC_GET_TRANSACTIONS_STATUS::response rsp = AUTO_VAL_INIT(rsp);

    req.transactionHashes = transactionHashes;

    m_logger(TRACE) << "Send get_transactions_status request";

    std::error_code ec = jsonCommand("/get_transactions_status", req, rsp);

    if (!ec)
    {
        m_logger(TRACE) << "get_transactions_status complete";
    }
    else
    {
        m_logger(TRACE) << "get_transactions_status failed: " << ec << ", " << ec.message();
    }

    transactionsInPool = rsp.transactionsInPool;
    transactionsInBlock = rsp.transactionsInBlock;
    transactionsUnknown = rsp.transactionsUnknown;

    return ec;
}

std::error_code NodeRpcProxy::doQueryBlocksLite(const std::vector<Crypto::Hash>& knownBlockIds, uint64_t timestamp,
        std::vector<CryptoNote::BlockShortEntry>& newBlocks, uint32_t& startHeight) {
  CryptoNote::COMMAND_RPC_QUERY_BLOCKS_LITE::request req = AUTO_VAL_INIT(req);
  CryptoNote::COMMAND_RPC_QUERY_BLOCKS_LITE::response rsp = AUTO_VAL_INIT(rsp);

  req.blockIds = knownBlockIds;
  req.timestamp = timestamp;

  m_logger(TRACE) << "Send queryblockslite request, timestamp " << req.timestamp;
  std::error_code ec = jsonCommand("/queryblockslite", req, rsp);
  if (ec) {
    m_logger(TRACE) << "queryblockslite failed: " << ec << ", " << ec.message();
    return ec;
  }

  m_logger(TRACE) << "queryblockslite complete, startHeight " << rsp.startHeight << ", block count " << rsp.items.size();
  startHeight = static_cast<uint32_t>(rsp.startHeight);

  for (auto& item: rsp.items) {
    BlockShortEntry bse;
    bse.hasBlock = false;

    bse.blockHash = std::move(item.blockId);
    if (!item.block.empty()) {
      if (!fromBinaryArray(bse.block, item.block)) {
        return std::make_error_code(std::errc::invalid_argument);
      }

      bse.hasBlock = true;
    }

    for (const auto& txp: item.txPrefixes) {
      TransactionShortInfo tsi;
      tsi.txId = txp.txHash;
      tsi.txPrefix = txp.txPrefix;
      bse.txsShortInfo.push_back(std::move(tsi));
    }

    newBlocks.push_back(std::move(bse));
  }

  return std::error_code();
}

std::error_code NodeRpcProxy::doGetWalletSyncData(const std::vector<Crypto::Hash>& knownBlockIds, uint64_t startHeight, uint64_t startTimestamp, std::vector<WalletTypes::WalletBlockInfo>& newBlocks) {

  CryptoNote::COMMAND_RPC_GET_WALLET_SYNC_DATA::request req = AUTO_VAL_INIT(req);
  CryptoNote::COMMAND_RPC_GET_WALLET_SYNC_DATA::response rsp = AUTO_VAL_INIT(rsp);

  req.blockIds = knownBlockIds;
  req.startHeight = startHeight;
  req.startTimestamp = startTimestamp;

  m_logger(TRACE) << "Send getwalletsyncdata request, start timestamp: " << req.startTimestamp
                  << ", start height: " << req.startHeight;

  std::error_code ec = jsonCommand("/getwalletsyncdata", req, rsp);
  if (ec) {
    m_logger(TRACE) << "getwalletsyncdata failed: " << ec << ", " << ec.message();
    return ec;
  }

  m_logger(TRACE) << "queryblockslite complete, block count " << rsp.items.size();

  newBlocks = rsp.items;

  return std::error_code();
}


std::error_code NodeRpcProxy::doGetPoolSymmetricDifference(std::vector<Crypto::Hash>&& knownPoolTxIds, Crypto::Hash knownBlockId, bool& isBcActual,
        std::vector<std::unique_ptr<ITransactionReader>>& newTxs, std::vector<Crypto::Hash>& deletedTxIds) {
  CryptoNote::COMMAND_RPC_GET_POOL_CHANGES_LITE::request req = AUTO_VAL_INIT(req);
  CryptoNote::COMMAND_RPC_GET_POOL_CHANGES_LITE::response rsp = AUTO_VAL_INIT(rsp);

  req.tailBlockId = knownBlockId;
  req.knownTxsIds = knownPoolTxIds;

  m_logger(TRACE) << "Send get_pool_changes_lite request, tailBlockId " << req.tailBlockId;
  std::error_code ec = jsonCommand("/get_pool_changes_lite", req, rsp);

  if (ec) {
    m_logger(TRACE) << "get_pool_changes_lite failed: " << ec << ", " << ec.message();
    return ec;
  }

  m_logger(TRACE) << "get_pool_changes_lite complete, isTailBlockActual " << rsp.isTailBlockActual;
  isBcActual = rsp.isTailBlockActual;

  deletedTxIds = std::move(rsp.deletedTxsIds);

  for (const auto& tpi : rsp.addedTxs) {
    newTxs.push_back(createTransactionPrefix(tpi.txPrefix, tpi.txHash));
  }

  return ec;
}

std::error_code NodeRpcProxy::doGetBlocksByHeight(const std::vector<uint32_t>& blockHeights, std::vector<std::vector<BlockDetails>>& blocks) {
  COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HEIGHTS::request req = AUTO_VAL_INIT(req);
  COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HEIGHTS::response resp = AUTO_VAL_INIT(resp);

  req.blockHeights = blockHeights;

  std::error_code ec = jsonCommand("/get_blocks_details_by_heights", req, resp);
  if (ec) {
    return ec;
  }

  auto tmp = std::move(resp.blocks);
  blocks.push_back(tmp);

  return ec;
}

std::error_code NodeRpcProxy::doGetBlocksByHash(const std::vector<Crypto::Hash>& blockHashes, std::vector<BlockDetails>& blocks) {
  COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES::request req = AUTO_VAL_INIT(req);
  COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES::response resp = AUTO_VAL_INIT(resp);

  req.blockHashes = blockHashes;

  std::error_code ec = jsonCommand("/get_blocks_details_by_hashes", req, resp);
  if (ec) {
    return ec;
  }

  blocks = std::move(resp.blocks);
  return ec;
}

std::error_code NodeRpcProxy::doGetBlock(const uint32_t blockHeight, BlockDetails& block) {
  COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT::request req = AUTO_VAL_INIT(req);
  COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT::response resp = AUTO_VAL_INIT(resp);

  req.blockHeight = blockHeight;

  std::error_code ec = jsonCommand("/get_block_details_by_height", req, resp);

  if (ec) {
    return ec;
  }

  block = std::move(resp.block);

  return ec;
}

std::error_code NodeRpcProxy::doGetTransactionHashesByPaymentId(const Crypto::Hash& paymentId, std::vector<Crypto::Hash>& transactionHashes) {
  COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID::request req = AUTO_VAL_INIT(req);
  COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID::response resp = AUTO_VAL_INIT(resp);

  req.paymentId = paymentId;
  std::error_code ec = jsonCommand("/get_transaction_hashes_by_payment_id", req, resp);
  if (ec) {
    return ec;
  }

  transactionHashes = std::move(resp.transactionHashes);
  return ec;
}

std::error_code NodeRpcProxy::doGetTransactions(const std::vector<Crypto::Hash>& transactionHashes, std::vector<TransactionDetails>& transactions) {
  COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASHES::request req = AUTO_VAL_INIT(req);
  COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASHES::response resp = AUTO_VAL_INIT(resp);

  req.transactionHashes = transactionHashes;
  std::error_code ec = jsonCommand("/get_transaction_details_by_hashes", req, resp);
  if (ec) {
    return ec;
  }

  transactions = std::move(resp.transactions);
  return ec;
}

void NodeRpcProxy::scheduleRequest(std::function<std::error_code()>&& procedure, const Callback& callback) {
  // callback is located on stack, so copy it inside binder
  class Wrapper {
  public:
    Wrapper(std::function<void(std::function<std::error_code()>&, Callback&)>&& _func,
            std::function<std::error_code()>&& _procedure, const Callback& _callback)
        : func(std::move(_func)), procedure(std::move(_procedure)), callback(std::move(_callback)) {
    }
    Wrapper(const Wrapper& other)
        : func(other.func), procedure(other.procedure), callback(other.callback) {
    }
    Wrapper(Wrapper&& other) // must be noexcept
        : func(std::move(other.func)), procedure(std::move(other.procedure)), callback(std::move(other.callback)) {
    }
    void operator()() {
      func(procedure, callback);
    }
  private:
    std::function<void(std::function<std::error_code()>&, Callback&)> func;
    std::function<std::error_code()> procedure;
    Callback callback;
  };
  assert(m_dispatcher != nullptr && m_context_group != nullptr);
  m_dispatcher->remoteSpawn(Wrapper([this](std::function<std::error_code()>& procedure, Callback& callback) {
    m_context_group->spawn(Wrapper([this](std::function<std::error_code()>& procedure, const Callback& callback) {
        if (m_stop) {
          callback(std::make_error_code(std::errc::operation_canceled));
        } else {
          std::error_code ec = procedure();
          if (m_connected != m_httpClient->isConnected()) {
            m_connected = m_httpClient->isConnected();
            m_rpcProxyObserverManager.notify(&INodeRpcProxyObserver::connectionStatusUpdated, m_connected);
          }
          callback(m_stop ? std::make_error_code(std::errc::operation_canceled) : ec);
        }
      }, std::move(procedure), std::move(callback)));
    }, std::move(procedure), callback));
}

template <typename Request, typename Response>
std::error_code NodeRpcProxy::binaryCommand(const std::string& url, const Request& req, Response& res) {
  std::error_code ec;

  try {
    EventLock eventLock(*m_httpEvent);
    invokeBinaryCommand(*m_httpClient, url, req, res);
    ec = interpretResponseStatus(res.status);
  } catch (const ConnectException&) {
    ec = make_error_code(NodeError::CONNECT_ERROR);
  } catch (const std::exception&) {
    ec = make_error_code(NodeError::NETWORK_ERROR);
  }

  return ec;
}

template <typename Request, typename Response>
std::error_code NodeRpcProxy::jsonCommand(const std::string& url, const Request& req, Response& res) {
  std::error_code ec;

  try {
    m_logger(TRACE) << "Send " << url << " JSON request";
    EventLock eventLock(*m_httpEvent);
    invokeJsonCommand(*m_httpClient, url, req, res);
    ec = interpretResponseStatus(res.status);
  } catch (const ConnectException&) {
    ec = make_error_code(NodeError::CONNECT_ERROR);
  } catch (const std::exception&) {
    ec = make_error_code(NodeError::NETWORK_ERROR);
  }

  if (ec) {
    m_logger(TRACE) << url << " JSON request failed: " << ec << ", " << ec.message();
  } else {
    m_logger(TRACE) << url << " JSON request compete";
  }

  return ec;
}

template <typename Request, typename Response>
std::error_code NodeRpcProxy::jsonRpcCommand(const std::string& method, const Request& req, Response& res) {
  std::error_code ec = make_error_code(NodeError::INTERNAL_NODE_ERROR);

  try {
    m_logger(TRACE) << "Send " << method << " JSON RPC request";
    EventLock eventLock(*m_httpEvent);

    JsonRpc::JsonRpcRequest jsReq;

    jsReq.setMethod(method);
    jsReq.setParams(req);

    HttpRequest httpReq;
    HttpResponse httpRes;

    httpReq.addHeader("Content-Type", "application/json");
    httpReq.setUrl("/json_rpc");
    httpReq.setBody(jsReq.getBody());

    m_httpClient->request(httpReq, httpRes);

    JsonRpc::JsonRpcResponse jsRes;

    if (httpRes.getStatus() == HttpResponse::STATUS_200) {
      jsRes.parse(httpRes.getBody());
      if (jsRes.getResult(res)) {
        ec = interpretResponseStatus(res.status);
      }
    }
  } catch (const ConnectException&) {
    ec = make_error_code(NodeError::CONNECT_ERROR);
  } catch (const std::exception&) {
    ec = make_error_code(NodeError::NETWORK_ERROR);
  }

  if (ec) {
    m_logger(TRACE) << method << " JSON RPC request failed: " << ec << ", " << ec.message();
  } else {
    m_logger(TRACE) << method << " JSON RPC request compete";
  }

  return ec;
}

}
