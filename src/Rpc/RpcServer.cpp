// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018, The Karai Developers
//
// Please see the included LICENSE file for more information.

//////////////////////////
#include <Rpc/RpcServer.h>
//////////////////////////

#include <cmath>

#include <Common/StringTools.h>

#include <config/CryptoNoteConfig.h>

#include <CryptoNoteCore/Core.h>
#include <CryptoNoteCore/CryptoNoteTools.h>
#include <CryptoNoteCore/TransactionExtra.h>

#include <CryptoNoteProtocol/CryptoNoteProtocolHandlerCommon.h>

#include <future>

#include <P2p/NetNode.h>

#include <Rpc/CoreRpcServerErrorCodes.h>
#include <Rpc/JsonRpc.h>

#include "version.h"

#include <unordered_map>

#include <Utilities/FormatTools.h>

#undef ERROR

using namespace Logging;
using namespace Crypto;
using namespace Common;

namespace CryptoNote {

static inline void serialize(COMMAND_RPC_GET_BLOCKS_FAST::response& response, ISerializer &s) {
  KV_MEMBER(response.blocks)
  KV_MEMBER(response.start_height)
  KV_MEMBER(response.current_height)
  KV_MEMBER(response.status)
}

void serialize(BlockFullInfo& blockFullInfo, ISerializer& s) {
  KV_MEMBER(blockFullInfo.block_id);
  KV_MEMBER(blockFullInfo.block);
  s(blockFullInfo.transactions, "txs");
}

void serialize(TransactionPrefixInfo& transactionPrefixInfo, ISerializer& s) {
  KV_MEMBER(transactionPrefixInfo.txHash);
  KV_MEMBER(transactionPrefixInfo.txPrefix);
}

void serialize(BlockShortInfo& blockShortInfo, ISerializer& s) {
  KV_MEMBER(blockShortInfo.blockId);
  KV_MEMBER(blockShortInfo.block);
  KV_MEMBER(blockShortInfo.txPrefixes);
}

void serialize(WalletTypes::WalletBlockInfo &walletBlockInfo, ISerializer &s)
{
    s(walletBlockInfo.coinbaseTransaction, "coinbaseTX");
    s(walletBlockInfo.transactions, "transactions");
    s(walletBlockInfo.blockHeight, "blockHeight");
    s(walletBlockInfo.blockHash, "blockHash");
    s(walletBlockInfo.blockTimestamp, "blockTimestamp");
}

void serialize(WalletTypes::RawTransaction &rawTransaction, ISerializer &s)
{
    s(rawTransaction.keyInputs, "inputs");
    s(rawTransaction.paymentID, "paymentID");
    s(rawTransaction.keyOutputs, "outputs");
    s(rawTransaction.hash, "hash");
    s(rawTransaction.transactionPublicKey, "txPublicKey");
    s(rawTransaction.unlockTime, "unlockTime");
}

void serialize(WalletTypes::RawCoinbaseTransaction &rawCoinbaseTransaction, ISerializer &s)
{
    s(rawCoinbaseTransaction.keyOutputs, "outputs");
    s(rawCoinbaseTransaction.hash, "hash");
    s(rawCoinbaseTransaction.transactionPublicKey, "txPublicKey");
    s(rawCoinbaseTransaction.unlockTime, "unlockTime");
}

void serialize(WalletTypes::KeyOutput &keyOutput, ISerializer &s)
{
    s(keyOutput.key, "key");
    s(keyOutput.amount, "amount");
}

namespace {

template <typename Command>
RpcServer::HandlerFunction jsonMethod(bool (RpcServer::*handler)(typename Command::request const&, typename Command::response&)) {
  return [handler](RpcServer* obj, const HttpRequest& request, HttpResponse& response) {

    boost::value_initialized<typename Command::request> req;
    boost::value_initialized<typename Command::response> res;

    if (!loadFromJson(static_cast<typename Command::request&>(req), request.getBody())) {
      return false;
    }

    bool result = (obj->*handler)(req, res);
    for (const auto& cors_domain: obj->getCorsDomains()) {
      response.addHeader("Access-Control-Allow-Origin", cors_domain);
    }
    response.addHeader("Content-Type", "application/json");
    response.setBody(storeToJson(res.data()));
    return result;
  };
}


}

std::unordered_map<std::string, RpcServer::RpcHandler<RpcServer::HandlerFunction>> RpcServer::s_handlers = {
  // old json handlers - remove me in 2019
  { "/getinfo", { jsonMethod<COMMAND_RPC_GET_INFO>(&RpcServer::on_get_info), true } },
  { "/getheight", { jsonMethod<COMMAND_RPC_GET_HEIGHT>(&RpcServer::on_get_height), true } },
  { "/feeinfo", { jsonMethod<COMMAND_RPC_GET_FEE_ADDRESS>(&RpcServer::on_get_fee_info), true } },
  { "/getpeers", { jsonMethod<COMMAND_RPC_GET_PEERS>(&RpcServer::on_get_peers), true } },

  // new json handlers
  { "/info", { jsonMethod<COMMAND_RPC_GET_INFO>(&RpcServer::on_get_info), true } },
  { "/height", { jsonMethod<COMMAND_RPC_GET_HEIGHT>(&RpcServer::on_get_height), true } },
  { "/fee", { jsonMethod<COMMAND_RPC_GET_FEE_ADDRESS>(&RpcServer::on_get_fee_info), true } },
  { "/peers", { jsonMethod<COMMAND_RPC_GET_PEERS>(&RpcServer::on_get_peers), true } },

  { "/gettransactions", { jsonMethod<COMMAND_RPC_GET_TRANSACTIONS>(&RpcServer::on_get_transactions), false } },
  { "/sendrawtransaction", { jsonMethod<COMMAND_RPC_SEND_RAW_TX>(&RpcServer::on_send_raw_tx), false } },

  { "/getblocks", { jsonMethod<COMMAND_RPC_GET_BLOCKS_FAST>(&RpcServer::on_get_blocks), false } },
  { "/queryblocks", { jsonMethod<COMMAND_RPC_QUERY_BLOCKS>(&RpcServer::on_query_blocks), false } },
  { "/queryblockslite", { jsonMethod<COMMAND_RPC_QUERY_BLOCKS_LITE>(&RpcServer::on_query_blocks_lite), false } },
  { "/queryblocksdetailed", { jsonMethod<COMMAND_RPC_QUERY_BLOCKS_DETAILED>(&RpcServer::on_query_blocks_detailed), false } },
  { "/getwalletsyncdata", { jsonMethod<COMMAND_RPC_GET_WALLET_SYNC_DATA>(&RpcServer::on_get_wallet_sync_data), false} },
  { "/get_o_indexes", { jsonMethod<COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES>(&RpcServer::on_get_indexes), false } },
  { "/getrandom_outs", { jsonMethod<COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS>(&RpcServer::on_get_random_outs), false } },
  { "/get_pool_changes", { jsonMethod<COMMAND_RPC_GET_POOL_CHANGES>(&RpcServer::onGetPoolChanges), false } },
  { "/get_pool_changes_lite", { jsonMethod<COMMAND_RPC_GET_POOL_CHANGES_LITE>(&RpcServer::onGetPoolChangesLite), false } },
  { "/get_block_details_by_height", { jsonMethod<COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT>(&RpcServer::onGetBlockDetailsByHeight), false } },
  { "/get_blocks_details_by_heights", { jsonMethod<COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HEIGHTS>(&RpcServer::onGetBlocksDetailsByHeights), false } },
  { "/get_blocks_details_by_hashes", { jsonMethod<COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES>(&RpcServer::onGetBlocksDetailsByHashes), false } },
  { "/get_blocks_hashes_by_timestamps", { jsonMethod<COMMAND_RPC_GET_BLOCKS_HASHES_BY_TIMESTAMPS>(&RpcServer::onGetBlocksHashesByTimestamps), false } },
  { "/get_transaction_details_by_hashes", { jsonMethod<COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASHES>(&RpcServer::onGetTransactionDetailsByHashes), false } },
  { "/get_transaction_hashes_by_payment_id", { jsonMethod<COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID>(&RpcServer::onGetTransactionHashesByPaymentId), false } },
  { "/get_global_indexes_for_range", { jsonMethod<COMMAND_RPC_GET_GLOBAL_INDEXES_FOR_RANGE>(&RpcServer::onGetGlobalIndexesForRange), false} },
  { "/get_transactions_status", { jsonMethod<COMMAND_RPC_GET_TRANSACTIONS_STATUS>(&RpcServer::onGetTransactionsStatus), false} },

  // json rpc
  { "/json_rpc", { std::bind(&RpcServer::processJsonRpcRequest, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3), true } }
};

RpcServer::RpcServer(System::Dispatcher& dispatcher, std::shared_ptr<Logging::ILogger> log, Core& c, NodeServer& p2p, ICryptoNoteProtocolHandler& protocol) :
  HttpServer(dispatcher, log), logger(log, "RpcServer"), m_core(c), m_p2p(p2p), m_protocol(protocol) {
}

void RpcServer::processRequest(const HttpRequest& request, HttpResponse& response) {
  auto url = request.getUrl();
  if (url.find(".bin") == std::string::npos) {
      logger(TRACE) << "RPC request came: \n" << request << std::endl;
  } else {
      logger(TRACE) << "RPC request came: " << url << std::endl;
  }

  auto it = s_handlers.find(url);
  if (it == s_handlers.end()) {
    response.setStatus(HttpResponse::STATUS_404);
    return;
  }

  if (!it->second.allowBusyCore && !isCoreReady()) {
    response.setStatus(HttpResponse::STATUS_500);
    response.setBody("Core is busy");
    return;
  }

  it->second.handler(this, request, response);
}

bool RpcServer::processJsonRpcRequest(const HttpRequest& request, HttpResponse& response) {

  using namespace JsonRpc;

  for (const auto& cors_domain: m_cors_domains) {
    response.addHeader("Access-Control-Allow-Origin", cors_domain);
  }
  response.addHeader("Content-Type", "application/json");

  JsonRpcRequest jsonRequest;
  JsonRpcResponse jsonResponse;

  try {
    logger(TRACE) << "JSON-RPC request: " << request.getBody();
    jsonRequest.parseRequest(request.getBody());
    jsonResponse.setId(jsonRequest.getId()); // copy id

    static std::unordered_map<std::string, RpcServer::RpcHandler<JsonMemberMethod>> jsonRpcHandlers = {
      { "f_blocks_list_json", { makeMemberMethod(&RpcServer::f_on_blocks_list_json), false } },
      { "f_block_json", { makeMemberMethod(&RpcServer::f_on_block_json), false } },
      { "f_transaction_json", { makeMemberMethod(&RpcServer::f_on_transaction_json), false } },
      { "f_on_transactions_pool_json", { makeMemberMethod(&RpcServer::f_on_transactions_pool_json), false } },
      { "getblockcount", { makeMemberMethod(&RpcServer::on_getblockcount), true } },
      { "on_getblockhash", { makeMemberMethod(&RpcServer::on_getblockhash), false } },
      { "getblocktemplate", { makeMemberMethod(&RpcServer::on_getblocktemplate), false } },
      { "getcurrencyid", { makeMemberMethod(&RpcServer::on_get_currency_id), true } },
      { "submitblock", { makeMemberMethod(&RpcServer::on_submitblock), false } },
      { "getlastblockheader", { makeMemberMethod(&RpcServer::on_get_last_block_header), false } },
      { "getblockheaderbyhash", { makeMemberMethod(&RpcServer::on_get_block_header_by_hash), false } },
      { "getblockheaderbyheight", { makeMemberMethod(&RpcServer::on_get_block_header_by_height), false } }
    };

    auto it = jsonRpcHandlers.find(jsonRequest.getMethod());
    if (it == jsonRpcHandlers.end()) {
      throw JsonRpcError(JsonRpc::errMethodNotFound);
    }

    if (!it->second.allowBusyCore && !isCoreReady()) {
      throw JsonRpcError(CORE_RPC_ERROR_CODE_CORE_BUSY, "Core is busy");
    }

    it->second.handler(this, jsonRequest, jsonResponse);

  } catch (const JsonRpcError& err) {
    jsonResponse.setError(err);
  } catch (const std::exception& e) {
    jsonResponse.setError(JsonRpcError(JsonRpc::errInternalError, e.what()));
  }

  response.setBody(jsonResponse.getBody());
  logger(TRACE) << "JSON-RPC response: " << jsonResponse.getBody();
  return true;
}

bool RpcServer::setFeeAddress(const std::string fee_address) {
  m_fee_address = fee_address;
  return true;
}

bool RpcServer::setFeeAmount(const uint32_t fee_amount) {
  m_fee_amount = fee_amount;
  return true;
}

bool RpcServer::enableCors(const std::vector<std::string> domains) {
  m_cors_domains = domains;
  return true;
}

std::vector<std::string> RpcServer::getCorsDomains() {
  return m_cors_domains;
}

bool RpcServer::isCoreReady() {
  return m_core.getCurrency().isTestnet() || m_p2p.get_payload_object().isSynchronized();
}

bool RpcServer::on_get_blocks(const COMMAND_RPC_GET_BLOCKS_FAST::request& req, COMMAND_RPC_GET_BLOCKS_FAST::response& res) {
  // TODO code duplication see InProcessNode::doGetNewBlocks()
  if (req.block_ids.empty()) {
    res.status = "Failed";
    return false;
  }

  if (req.block_ids.back() != m_core.getBlockHashByIndex(0)) {
    res.status = "Failed";
    return false;
  }

  uint32_t totalBlockCount;
  uint32_t startBlockIndex;
  std::vector<Crypto::Hash> supplement = m_core.findBlockchainSupplement(req.block_ids, COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT, totalBlockCount, startBlockIndex);

  res.current_height = totalBlockCount;
  res.start_height = startBlockIndex;

  std::vector<Crypto::Hash> missedHashes;
  m_core.getBlocks(supplement, res.blocks, missedHashes);
  assert(missedHashes.empty());

  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_query_blocks(const COMMAND_RPC_QUERY_BLOCKS::request& req, COMMAND_RPC_QUERY_BLOCKS::response& res) {
  uint32_t startIndex;
  uint32_t currentIndex;
  uint32_t fullOffset;

  if (!m_core.queryBlocks(req.block_ids, req.timestamp, startIndex, currentIndex, fullOffset, res.items)) {
    res.status = "Failed to perform query";
    return false;
  }

  res.start_height = startIndex + 1;
  res.current_height = currentIndex + 1;
  res.full_offset = fullOffset;
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_query_blocks_lite(const COMMAND_RPC_QUERY_BLOCKS_LITE::request& req, COMMAND_RPC_QUERY_BLOCKS_LITE::response& res) {
  uint32_t startIndex;
  uint32_t currentIndex;
  uint32_t fullOffset;
  if (!m_core.queryBlocksLite(req.blockIds, req.timestamp, startIndex, currentIndex, fullOffset, res.items)) {
    res.status = "Failed to perform query";
    return false;
  }

  res.startHeight = startIndex;
  res.currentHeight = currentIndex;
  res.fullOffset = fullOffset;
  res.status = CORE_RPC_STATUS_OK;

  return true;
}

bool RpcServer::on_query_blocks_detailed(const COMMAND_RPC_QUERY_BLOCKS_DETAILED::request& req, COMMAND_RPC_QUERY_BLOCKS_DETAILED::response& res) {
  uint64_t startIndex;
  uint64_t currentIndex;
  uint64_t fullOffset;

  if (!m_core.queryBlocksDetailed(req.blockIds, req.timestamp, startIndex, currentIndex, fullOffset, res.blocks, req.blockCount))
  {
    res.status = "Failed to perform query";
    return false;
  }

  res.startHeight = startIndex;
  res.currentHeight = currentIndex;
  res.fullOffset = fullOffset;
  res.status = CORE_RPC_STATUS_OK;

  return true;
}

bool RpcServer::on_get_wallet_sync_data(const COMMAND_RPC_GET_WALLET_SYNC_DATA::request &req, COMMAND_RPC_GET_WALLET_SYNC_DATA::response &res)
{
    if (!m_core.getWalletSyncData(req.blockIds, req.startHeight, req.startTimestamp, req.blockCount, res.items))
    {
        res.status = "Failed to perform query";
        return false;
    }

    res.status = CORE_RPC_STATUS_OK;

    return true;
}

bool RpcServer::onGetTransactionsStatus(
    const COMMAND_RPC_GET_TRANSACTIONS_STATUS::request &req,
    COMMAND_RPC_GET_TRANSACTIONS_STATUS::response &res)
{
    if (!m_core.getTransactionsStatus(req.transactionHashes, res.transactionsInPool, res.transactionsInBlock, res.transactionsUnknown))
    {
        res.status = "Failed to perform query";
        return false;
    }

    res.status = CORE_RPC_STATUS_OK;

    return true;
}

bool RpcServer::on_get_indexes(const COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::request& req, COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES::response& res) {
  std::vector<uint32_t> outputIndexes;
  if (!m_core.getTransactionGlobalIndexes(req.txid, outputIndexes)) {
    res.status = "Failed";
    return true;
  }

  res.o_indexes.assign(outputIndexes.begin(), outputIndexes.end());
  res.status = CORE_RPC_STATUS_OK;
  logger(TRACE) << "COMMAND_RPC_GET_TX_GLOBAL_OUTPUTS_INDEXES: [" << res.o_indexes.size() << "]";
  return true;
}

bool RpcServer::onGetGlobalIndexesForRange(
    const COMMAND_RPC_GET_GLOBAL_INDEXES_FOR_RANGE::request &req,
    COMMAND_RPC_GET_GLOBAL_INDEXES_FOR_RANGE::response &res)
{
    if (!m_core.getGlobalIndexesForRange(req.startHeight, req.endHeight, res.indexes))
    {
        res.status = "Failed";
        return true;
    }

    res.status = CORE_RPC_STATUS_OK;

    return true;
}

bool RpcServer::on_get_random_outs(const COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::request& req, COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::response& res) {
  res.status = "Failed";

  for (uint64_t amount : req.amounts) {
    std::vector<uint32_t> globalIndexes;
    std::vector<Crypto::PublicKey> publicKeys;
    if (!m_core.getRandomOutputs(amount, static_cast<uint16_t>(req.outs_count), globalIndexes, publicKeys)) {
      return true;
    }

    if (globalIndexes.size() != req.outs_count)
    {
        logger(ERROR) << "Failed to get enough matching outputs for amount "
                      << amount << " (" << Utilities::formatAmount(amount)
                      << "). Requested outputs: " << req.outs_count
                      << ", found outputs: " << globalIndexes.size()
                      << ". Further explanation here: https://gist.github.com/zpalmtree/80b3e80463225bcfb8f8432043cb594c"
                      << std::endl
                      << "Note: If you are a public node operator, you can safely ignore this message. "
                      << "It is only relevant to the user sending the transaction.";
    }

    assert(globalIndexes.size() == publicKeys.size());
    res.outs.push_back({amount, {}});
    for (size_t i = 0; i < globalIndexes.size(); ++i) {
      res.outs.back().outs.push_back({globalIndexes[i], publicKeys[i]});
    }
  }

  res.status = CORE_RPC_STATUS_OK;

  std::stringstream ss;

  std::for_each(res.outs.begin(), res.outs.end(), [&](auto& ofa)  {
    ss << "[" << ofa.amount << "]:";

    assert(ofa.outs.size() && "internal error: ofa.outs.size() is empty");

    std::for_each(ofa.outs.begin(), ofa.outs.end(), [&](auto& oe)
    {
      ss << oe.global_amount_index << " ";
    });
    ss << ENDL;
  });
  std::string s = ss.str();
  logger(TRACE) << "COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS: " << ENDL << s;
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::onGetPoolChanges(const COMMAND_RPC_GET_POOL_CHANGES::request& req, COMMAND_RPC_GET_POOL_CHANGES::response& rsp) {
  rsp.status = CORE_RPC_STATUS_OK;
  rsp.isTailBlockActual = m_core.getPoolChanges(req.tailBlockId, req.knownTxsIds, rsp.addedTxs, rsp.deletedTxsIds);

  return true;
}

bool RpcServer::onGetPoolChangesLite(const COMMAND_RPC_GET_POOL_CHANGES_LITE::request& req, COMMAND_RPC_GET_POOL_CHANGES_LITE::response& rsp) {
  rsp.status = CORE_RPC_STATUS_OK;
  rsp.isTailBlockActual = m_core.getPoolChangesLite(req.tailBlockId, req.knownTxsIds, rsp.addedTxs, rsp.deletedTxsIds);

  return true;
}

bool RpcServer::onGetBlocksDetailsByHeights(const COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HEIGHTS::request& req, COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HEIGHTS::response& rsp) {
  try {
    std::vector<BlockDetails> blockDetails;
    for (const uint32_t& height : req.blockHeights) {
      blockDetails.push_back(m_core.getBlockDetails(height));
    }

    rsp.blocks = std::move(blockDetails);
  } catch (std::system_error& e) {
    rsp.status = e.what();
    return false;
  } catch (std::exception& e) {
    rsp.status = "Error: " + std::string(e.what());
    return false;
  }

  rsp.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::onGetBlocksDetailsByHashes(const COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES::request& req, COMMAND_RPC_GET_BLOCKS_DETAILS_BY_HASHES::response& rsp) {
  try {
    std::vector<BlockDetails> blockDetails;
    for (const Crypto::Hash& hash : req.blockHashes) {
      blockDetails.push_back(m_core.getBlockDetails(hash));
    }

    rsp.blocks = std::move(blockDetails);
  } catch (std::system_error& e) {
    rsp.status = e.what();
    return false;
  } catch (std::exception& e) {
    rsp.status = "Error: " + std::string(e.what());
    return false;
  }

  rsp.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::onGetBlockDetailsByHeight(const COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT::request& req, COMMAND_RPC_GET_BLOCK_DETAILS_BY_HEIGHT::response& rsp) {
  try {
    BlockDetails blockDetails = m_core.getBlockDetails(req.blockHeight);
    rsp.block = blockDetails;
  } catch (std::system_error& e) {
    rsp.status = e.what();
    return false;
  } catch (std::exception& e) {
    rsp.status = "Error: " + std::string(e.what());
    return false;
  }

  rsp.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::onGetBlocksHashesByTimestamps(const COMMAND_RPC_GET_BLOCKS_HASHES_BY_TIMESTAMPS::request& req, COMMAND_RPC_GET_BLOCKS_HASHES_BY_TIMESTAMPS::response& rsp) {
  try {
    auto blockHashes = m_core.getBlockHashesByTimestamps(req.timestampBegin, req.secondsCount);
    rsp.blockHashes = std::move(blockHashes);
  } catch (std::system_error& e) {
    rsp.status = e.what();
    return false;
  } catch (std::exception& e) {
    rsp.status = "Error: " + std::string(e.what());
    return false;
  }

  rsp.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::onGetTransactionDetailsByHashes(const COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASHES::request& req, COMMAND_RPC_GET_TRANSACTION_DETAILS_BY_HASHES::response& rsp) {
  try {
    std::vector<TransactionDetails> transactionDetails;
    transactionDetails.reserve(req.transactionHashes.size());

    for (const auto& hash: req.transactionHashes) {
      transactionDetails.push_back(m_core.getTransactionDetails(hash));
    }

    rsp.transactions = std::move(transactionDetails);
  } catch (std::system_error& e) {
    rsp.status = e.what();
    return false;
  } catch (std::exception& e) {
    rsp.status = "Error: " + std::string(e.what());
    return false;
  }

  rsp.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::onGetTransactionHashesByPaymentId(const COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID::request& req, COMMAND_RPC_GET_TRANSACTION_HASHES_BY_PAYMENT_ID::response& rsp) {
  try {
    rsp.transactionHashes = m_core.getTransactionHashesByPaymentId(req.paymentId);
  } catch (std::system_error& e) {
    rsp.status = e.what();
    return false;
  } catch (std::exception& e) {
    rsp.status = "Error: " + std::string(e.what());
    return false;
  }

  rsp.status = CORE_RPC_STATUS_OK;
  return true;
}

//
// JSON handlers
//

bool RpcServer::on_get_info(const COMMAND_RPC_GET_INFO::request& req, COMMAND_RPC_GET_INFO::response& res) {
  res.height = m_core.getTopBlockIndex() + 1;
  res.difficulty = m_core.getDifficultyForNextBlock();
  res.tx_count = m_core.getBlockchainTransactionCount() - res.height; //without coinbase
  res.tx_pool_size = m_core.getPoolTransactionCount();
  res.alt_blocks_count = m_core.getAlternativeBlockCount();
  uint64_t total_conn = m_p2p.get_connections_count();
  res.outgoing_connections_count = m_p2p.get_outgoing_connections_count();
  res.incoming_connections_count = total_conn - res.outgoing_connections_count;
  res.white_peerlist_size = m_p2p.getPeerlistManager().get_white_peers_count();
  res.grey_peerlist_size = m_p2p.getPeerlistManager().get_gray_peers_count();
  res.last_known_block_index = std::max(static_cast<uint32_t>(1), m_protocol.getObservedHeight()) - 1;
  res.network_height = std::max(static_cast<uint32_t>(1), m_protocol.getBlockchainHeight());
  res.upgrade_heights = CryptoNote::parameters::FORK_HEIGHTS_SIZE == 0 ? std::vector<uint64_t>() : std::vector<uint64_t>(CryptoNote::parameters::FORK_HEIGHTS, CryptoNote::parameters::FORK_HEIGHTS + CryptoNote::parameters::FORK_HEIGHTS_SIZE);
  res.supported_height = CryptoNote::parameters::FORK_HEIGHTS_SIZE == 0 ? 0 : CryptoNote::parameters::FORK_HEIGHTS[CryptoNote::parameters::CURRENT_FORK_INDEX];
  res.hashrate = (uint32_t)round(res.difficulty / CryptoNote::parameters::DIFFICULTY_TARGET);
  res.synced = ((uint64_t)res.height == (uint64_t)res.network_height);
  res.testnet = m_core.getCurrency().isTestnet();
  res.major_version = m_core.getBlockDetails(m_core.getTopBlockIndex()).majorVersion;
  res.minor_version = m_core.getBlockDetails(m_core.getTopBlockIndex()).minorVersion;
  res.version = PROJECT_VERSION;
  res.status = CORE_RPC_STATUS_OK;
  res.start_time = (uint64_t)m_core.getStartTime();
  return true;
}

bool RpcServer::on_get_height(const COMMAND_RPC_GET_HEIGHT::request& req, COMMAND_RPC_GET_HEIGHT::response& res) {
  res.height = m_core.getTopBlockIndex() + 1;
  res.network_height = std::max(static_cast<uint32_t>(1), m_protocol.getBlockchainHeight());
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_get_transactions(const COMMAND_RPC_GET_TRANSACTIONS::request& req, COMMAND_RPC_GET_TRANSACTIONS::response& res) {
  std::vector<Hash> vh;
  for (const auto& tx_hex_str : req.txs_hashes) {
    BinaryArray b;
    if (!fromHex(tx_hex_str, b)) {
      res.status = "Failed to parse hex representation of transaction hash";
      return true;
    }

    if (b.size() != sizeof(Hash)) {
      res.status = "Failed, size of data mismatch";
    }

    vh.push_back(*reinterpret_cast<const Hash*>(b.data()));
  }

  std::vector<Hash> missed_txs;
  std::vector<BinaryArray> txs;
  m_core.getTransactions(vh, txs, missed_txs);

  for (auto& tx : txs) {
    res.txs_as_hex.push_back(toHex(tx));
  }

  for (const auto& miss_tx : missed_txs) {
    res.missed_tx.push_back(Common::podToHex(miss_tx));
  }

  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_send_raw_tx(const COMMAND_RPC_SEND_RAW_TX::request& req, COMMAND_RPC_SEND_RAW_TX::response& res) {
  std::vector<BinaryArray> transactions(1);
  if (!fromHex(req.tx_as_hex, transactions.back())) {
    logger(INFO) << "[on_send_raw_tx]: Failed to parse tx from hexbuff: " << req.tx_as_hex;
    res.status = "Failed";
    return true;
  }

  Crypto::Hash transactionHash = Crypto::cn_fast_hash(transactions.back().data(), transactions.back().size());
  logger(DEBUGGING) << "transaction " << transactionHash << " came in on_send_raw_tx";

  if (!m_core.addTransactionToPool(transactions.back())) {
    logger(DEBUGGING) << "[on_send_raw_tx]: tx verification failed";
    res.status = "Failed";
    return true;
  }

  m_protocol.relayTransactions(transactions);
  //TODO: make sure that tx has reached other nodes here, probably wait to receive reflections from other nodes
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_get_fee_info(const COMMAND_RPC_GET_FEE_ADDRESS::request & req, COMMAND_RPC_GET_FEE_ADDRESS::response & res) {
  if (m_fee_address.empty()) {
    res.status = CORE_RPC_STATUS_OK;
    return false;
  }

  res.address = m_fee_address;
  res.amount = m_fee_amount;
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_get_peers(const COMMAND_RPC_GET_PEERS::request& req, COMMAND_RPC_GET_PEERS::response& res) {
  std::list<PeerlistEntry> peers_white;
  std::list<PeerlistEntry> peers_gray;

  m_p2p.getPeerlistManager().get_peerlist_full(peers_gray, peers_white);

  for (const auto& peer : peers_white) {
    std::stringstream stream;
    stream << peer.adr;
    res.peers.push_back(stream.str());
  }

  for (const auto& peer : peers_gray) {
    std::stringstream stream;
    stream << peer.adr;
    res.gray_peers.push_back(stream.str());
  }

  res.status = CORE_RPC_STATUS_OK;
  return true;
}

//------------------------------------------------------------------------------------------------------------------------------
// JSON RPC methods
//------------------------------------------------------------------------------------------------------------------------------
bool RpcServer::f_on_blocks_list_json(const F_COMMAND_RPC_GET_BLOCKS_LIST::request& req, F_COMMAND_RPC_GET_BLOCKS_LIST::response& res) {
  // check if blockchain explorer RPC is enabled
  if (m_core.getCurrency().isBlockexplorer() == false) {
    return false;
  }

  if (m_core.getTopBlockIndex() + 1 <= req.height) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT,
      std::string("To big height: ") + std::to_string(req.height) + ", current blockchain height = " + std::to_string(m_core.getTopBlockIndex()) };
  }

  uint32_t print_blocks_count = 30;
  uint32_t last_height = static_cast<uint32_t>(req.height - print_blocks_count);
  if (req.height <= print_blocks_count)  {
    last_height = 0;
  }

  for (uint32_t i = static_cast<uint32_t>(req.height); i >= last_height; i--) {
    Hash block_hash = m_core.getBlockHashByIndex(static_cast<uint32_t>(i));
    if (!m_core.hasBlock(block_hash)) {
      throw JsonRpc::JsonRpcError{
        CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
        "Internal error: can't get block by height. Height = " + std::to_string(i) + '.' };
    }
    BlockTemplate blk = m_core.getBlockByHash(block_hash);
    BlockDetails blkDetails = m_core.getBlockDetails(block_hash);

    f_block_short_response block_short;
    block_short.cumul_size = blkDetails.blockSize;
    block_short.timestamp = blk.timestamp;
    block_short.difficulty = blkDetails.difficulty;
    block_short.height = i;
    block_short.hash = Common::podToHex(block_hash);
    block_short.tx_count = blk.transactionHashes.size() + 1;

    res.blocks.push_back(block_short);

    if (i == 0)
      break;
  }

  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::f_on_block_json(const F_COMMAND_RPC_GET_BLOCK_DETAILS::request& req, F_COMMAND_RPC_GET_BLOCK_DETAILS::response& res) {
  // check if blockchain explorer RPC is enabled
  if (m_core.getCurrency().isBlockexplorer() == false) {
    // NOTE I think this should set a log error
    return false;
  }

  Hash hash;

  try {
    uint32_t height = boost::lexical_cast<uint32_t>(req.hash);
    hash = m_core.getBlockHashByIndex(height);
  } catch (boost::bad_lexical_cast &) {
    if (!parse_hash256(req.hash, hash)) {
      throw JsonRpc::JsonRpcError{
        CORE_RPC_ERROR_CODE_WRONG_PARAM,
        "Failed to parse hex representation of block hash. Hex = " + req.hash + '.' };
    }
  }

  if (!m_core.hasBlock(hash)) {
    throw JsonRpc::JsonRpcError{
      CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
      "Internal error: can't get block by hash. Hash = " + req.hash + '.' };
  }
  BlockTemplate blk = m_core.getBlockByHash(hash);
  BlockDetails blkDetails = m_core.getBlockDetails(hash);

  if (blk.baseTransaction.inputs.front().type() != typeid(BaseInput)) {
    throw JsonRpc::JsonRpcError{
      CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
      "Internal error: coinbase transaction in the block has the wrong type" };
  }

  block_header_response block_header;
  res.block.height = boost::get<BaseInput>(blk.baseTransaction.inputs.front()).blockIndex;
  fill_block_header_response(blk, false, res.block.height, hash, block_header);

  res.block.major_version = block_header.major_version;
  res.block.minor_version = block_header.minor_version;
  res.block.timestamp = block_header.timestamp;
  res.block.prev_hash = block_header.prev_hash;
  res.block.nonce = block_header.nonce;
  res.block.hash = Common::podToHex(hash);
  res.block.depth = m_core.getTopBlockIndex() - res.block.height;
  res.block.difficulty = m_core.getBlockDifficulty(res.block.height);
  res.block.transactionsCumulativeSize = blkDetails.transactionsCumulativeSize;
  res.block.alreadyGeneratedCoins = std::to_string(blkDetails.alreadyGeneratedCoins);
  res.block.alreadyGeneratedTransactions = blkDetails.alreadyGeneratedTransactions;
  res.block.reward = block_header.reward;
  res.block.sizeMedian = blkDetails.sizeMedian;
  res.block.blockSize = blkDetails.blockSize;
  res.block.orphan_status = blkDetails.isAlternative;

  uint64_t maxReward = 0;
  uint64_t currentReward = 0;
  int64_t emissionChange = 0;

  if (maxReward) {}
  if (currentReward) {}
  if (emissionChange) {}

  uint64_t blockGrantedFullRewardZone = m_core.getCurrency().blockGrantedFullRewardZoneByBlockVersion(block_header.major_version);
  res.block.effectiveSizeMedian = std::max(res.block.sizeMedian, blockGrantedFullRewardZone);

  res.block.baseReward = blkDetails.baseReward;
  res.block.penalty = blkDetails.penalty;

  // Base transaction adding
  f_transaction_short_response transaction_short;
  transaction_short.hash = Common::podToHex(getObjectHash(blk.baseTransaction));
  transaction_short.fee = 0;
  transaction_short.amount_out = getOutputAmount(blk.baseTransaction);
  transaction_short.size = getObjectBinarySize(blk.baseTransaction);
  res.block.transactions.push_back(transaction_short);

  std::vector<Crypto::Hash> missed_txs;
  std::vector<BinaryArray> txs;
  m_core.getTransactions(blk.transactionHashes, txs, missed_txs);

  res.block.totalFeeAmount = 0;

  for (const BinaryArray& ba : txs) {
    Transaction tx;
    if (!fromBinaryArray(tx, ba)) {
      throw std::runtime_error("Couldn't deserialize transaction");
    }
    f_transaction_short_response transaction_short;
    uint64_t amount_in = getInputAmount(tx);
    uint64_t amount_out = getOutputAmount(tx);

    transaction_short.hash = Common::podToHex(getObjectHash(tx));
    transaction_short.fee = amount_in - amount_out;
    transaction_short.amount_out = amount_out;
    transaction_short.size = getObjectBinarySize(tx);
    res.block.transactions.push_back(transaction_short);

    res.block.totalFeeAmount += transaction_short.fee;
  }

  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::f_on_transaction_json(const F_COMMAND_RPC_GET_TRANSACTION_DETAILS::request& req, F_COMMAND_RPC_GET_TRANSACTION_DETAILS::response& res) {
  // check if blockchain explorer RPC is enabled
  if (m_core.getCurrency().isBlockexplorer() == false) {
    return false;
  }

  Hash hash;

  if (!parse_hash256(req.hash, hash)) {
    throw JsonRpc::JsonRpcError{
      CORE_RPC_ERROR_CODE_WRONG_PARAM,
      "Failed to parse hex representation of transaction hash. Hex = " + req.hash + '.' };
  }

  std::vector<Crypto::Hash> tx_ids;
  tx_ids.push_back(hash);

  std::vector<Crypto::Hash> missed_txs;
  std::vector<BinaryArray> txs;
  m_core.getTransactions(tx_ids, txs, missed_txs);

  if (1 == txs.size()) {
    Transaction transaction;
    if (!fromBinaryArray(transaction, txs.front())) {
      throw std::runtime_error("Couldn't deserialize transaction");
    }
    res.tx = transaction;
  } else {
    throw JsonRpc::JsonRpcError{
      CORE_RPC_ERROR_CODE_WRONG_PARAM,
      "transaction wasn't found. Hash = " + req.hash + '.' };
  }
  TransactionDetails transactionDetails = m_core.getTransactionDetails(hash);

  Crypto::Hash blockHash;
  if (transactionDetails.inBlockchain) {
    uint32_t blockHeight = transactionDetails.blockIndex;
    if (!blockHeight) {
      throw JsonRpc::JsonRpcError{
        CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
        "Internal error: can't get transaction by hash. Hash = " + Common::podToHex(hash) + '.' };
    }
    blockHash = m_core.getBlockHashByIndex(blockHeight);
    BlockTemplate blk = m_core.getBlockByHash(blockHash);
    BlockDetails blkDetails = m_core.getBlockDetails(blockHash);

    f_block_short_response block_short;

    block_short.difficulty = blkDetails.difficulty;
    block_short.cumul_size = blkDetails.blockSize;
    block_short.timestamp = blk.timestamp;
    block_short.height = blockHeight;
    block_short.hash = Common::podToHex(blockHash);
    block_short.tx_count = blk.transactionHashes.size() + 1;
    res.block = block_short;
  }

  uint64_t amount_in = getInputAmount(res.tx);
  uint64_t amount_out = getOutputAmount(res.tx);

  res.txDetails.hash = Common::podToHex(getObjectHash(res.tx));
  res.txDetails.fee = amount_in - amount_out;
  if (amount_in == 0)
    res.txDetails.fee = 0;
  res.txDetails.amount_out = amount_out;
  res.txDetails.size = getObjectBinarySize(res.tx);

  uint64_t mixin;
  if (!f_getMixin(res.tx, mixin)) {
    return false;
  }
  res.txDetails.mixin = mixin;

  Crypto::Hash paymentId;
  if (CryptoNote::getPaymentIdFromTxExtra(res.tx.extra, paymentId)) {
    res.txDetails.paymentId = Common::podToHex(paymentId);
  } else {
    res.txDetails.paymentId = "";
  }

  res.status = CORE_RPC_STATUS_OK;
  return true;
}


bool RpcServer::f_on_transactions_pool_json(const F_COMMAND_RPC_GET_POOL::request& req, F_COMMAND_RPC_GET_POOL::response& res) {
  // check if blockchain explorer RPC is enabled
  if (m_core.getCurrency().isBlockexplorer() == false) {
    return false;
  }

  auto pool = m_core.getPoolTransactions();
  for (const Transaction tx : pool) {
    f_transaction_short_response transaction_short;
    uint64_t amount_in = getInputAmount(tx);
    uint64_t amount_out = getOutputAmount(tx);

    transaction_short.hash = Common::podToHex(getObjectHash(tx));
    transaction_short.fee = amount_in - amount_out;
    transaction_short.amount_out = amount_out;
    transaction_short.size = getObjectBinarySize(tx);
    res.transactions.push_back(transaction_short);
  }

  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::f_getMixin(const Transaction& transaction, uint64_t& mixin) {
  mixin = 0;
  for (const TransactionInput& txin : transaction.inputs) {
    if (txin.type() != typeid(KeyInput)) {
      continue;
    }
    uint64_t currentMixin = boost::get<KeyInput>(txin).outputIndexes.size();
    if (currentMixin > mixin) {
      mixin = currentMixin;
    }
  }
  return true;
}
bool RpcServer::on_getblockcount(const COMMAND_RPC_GETBLOCKCOUNT::request& req, COMMAND_RPC_GETBLOCKCOUNT::response& res) {
  res.count = m_core.getTopBlockIndex() + 1;
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_getblockhash(const COMMAND_RPC_GETBLOCKHASH::request& req, COMMAND_RPC_GETBLOCKHASH::response& res) {
  if (req.size() != 1) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_PARAM, "Wrong parameters, expected height" };
  }

  uint32_t h = static_cast<uint32_t>(req[0]);
  Crypto::Hash blockId = m_core.getBlockHashByIndex(h - 1);
  if (blockId == NULL_HASH) {
    throw JsonRpc::JsonRpcError{
      CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT,
      std::string("Too big height: ") + std::to_string(h) + ", current blockchain height = " + std::to_string(m_core.getTopBlockIndex() + 1)
    };
  }

  res = Common::podToHex(blockId);
  return true;
}

namespace {
  uint64_t slow_memmem(void* start_buff, size_t buflen, void* pat, size_t patlen)
  {
    void* buf = start_buff;
    void* end = (char*)buf + buflen - patlen;
    while ((buf = memchr(buf, ((char*)pat)[0], buflen)))
    {
      if (buf>end)
        return 0;
      if (memcmp(buf, pat, patlen) == 0)
        return (char*)buf - (char*)start_buff;
      buf = (char*)buf + 1;
    }
    return 0;
  }
}

bool RpcServer::on_getblocktemplate(const COMMAND_RPC_GETBLOCKTEMPLATE::request& req, COMMAND_RPC_GETBLOCKTEMPLATE::response& res) {
  if (req.reserve_size > TX_EXTRA_NONCE_MAX_COUNT) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_TOO_BIG_RESERVE_SIZE, "To big reserved size, maximum 255" };
  }

  AccountPublicAddress acc = boost::value_initialized<AccountPublicAddress>();

  if (!req.wallet_address.size() || !m_core.getCurrency().parseAccountAddressString(req.wallet_address, acc)) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_WALLET_ADDRESS, "Failed to parse wallet address" };
  }

  BlockTemplate blockTemplate = boost::value_initialized<BlockTemplate>();
  CryptoNote::BinaryArray blob_reserve;
  blob_reserve.resize(req.reserve_size, 0);

  if (!m_core.getBlockTemplate(blockTemplate, acc, blob_reserve, res.difficulty, res.height)) {
    logger(ERROR) << "Failed to create block template";
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Internal error: failed to create block template" };
  }

  BinaryArray block_blob = toBinaryArray(blockTemplate);
  PublicKey tx_pub_key = CryptoNote::getTransactionPublicKeyFromExtra(blockTemplate.baseTransaction.extra);
  if (tx_pub_key == NULL_PUBLIC_KEY) {
    logger(ERROR) << "Failed to find tx pub key in coinbase extra";
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Internal error: failed to find tx pub key in coinbase extra" };
  }

  if (0 < req.reserve_size) {
    res.reserved_offset = slow_memmem((void*)block_blob.data(), block_blob.size(), &tx_pub_key, sizeof(tx_pub_key));
    if (!res.reserved_offset) {
      logger(ERROR) << "Failed to find tx pub key in blockblob";
      throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Internal error: failed to create block template" };
    }
    res.reserved_offset += sizeof(tx_pub_key) + 3; //3 bytes: tag for TX_EXTRA_TAG_PUBKEY(1 byte), tag for TX_EXTRA_NONCE(1 byte), counter in TX_EXTRA_NONCE(1 byte)
    if (res.reserved_offset + req.reserve_size > block_blob.size()) {
      logger(ERROR) << "Failed to calculate offset for reserved bytes";
      throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_INTERNAL_ERROR, "Internal error: failed to create block template" };
    }
  } else {
    res.reserved_offset = 0;
  }

  res.blocktemplate_blob = toHex(block_blob);
  res.status = CORE_RPC_STATUS_OK;

  return true;
}

bool RpcServer::on_get_currency_id(const COMMAND_RPC_GET_CURRENCY_ID::request& /*req*/, COMMAND_RPC_GET_CURRENCY_ID::response& res) {
  Hash genesisBlockHash = m_core.getCurrency().genesisBlockHash();
  res.currency_id_blob = Common::podToHex(genesisBlockHash);
  return true;
}

bool RpcServer::on_submitblock(const COMMAND_RPC_SUBMITBLOCK::request& req, COMMAND_RPC_SUBMITBLOCK::response& res) {
  if (req.size() != 1) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_PARAM, "Wrong param" };
  }

  BinaryArray blockblob;
  if (!fromHex(req[0], blockblob)) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_WRONG_BLOCKBLOB, "Wrong block blob" };
  }

  auto blockToSend = blockblob;
  auto submitResult = m_core.submitBlock(std::move(blockblob));
  if (submitResult != error::AddBlockErrorCondition::BLOCK_ADDED) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_BLOCK_NOT_ACCEPTED, "Block not accepted" };
  }

  if (submitResult == error::AddBlockErrorCode::ADDED_TO_MAIN
      || submitResult == error::AddBlockErrorCode::ADDED_TO_ALTERNATIVE_AND_SWITCHED) {
    NOTIFY_NEW_BLOCK::request newBlockMessage;
    newBlockMessage.block = prepareRawBlockLegacy(std::move(blockToSend));
    newBlockMessage.hop = 0;
    newBlockMessage.current_blockchain_height = m_core.getTopBlockIndex() + 1; //+1 because previous version of core sent m_blocks.size()

    m_protocol.relayBlock(newBlockMessage);
  }

  res.status = CORE_RPC_STATUS_OK;
  return true;
}

RawBlockLegacy RpcServer::prepareRawBlockLegacy(BinaryArray&& blockBlob) {
  BlockTemplate blockTemplate;
  bool result = fromBinaryArray(blockTemplate, blockBlob);
  if (result) {}
  assert(result);

  RawBlockLegacy rawBlock;
  rawBlock.blockTemplate = std::move(blockBlob);

  if (blockTemplate.transactionHashes.empty()) {
    return rawBlock;
  }

  rawBlock.transactions.reserve(blockTemplate.transactionHashes.size());
  std::vector<Crypto::Hash> missedTransactions;
  m_core.getTransactions(blockTemplate.transactionHashes, rawBlock.transactions, missedTransactions);
  assert(missedTransactions.empty());

  return rawBlock;
}

namespace {

uint64_t get_block_reward(const BlockTemplate& blk) {
  uint64_t reward = 0;
  for (const TransactionOutput& out : blk.baseTransaction.outputs) {
    reward += out.amount;
  }

  return reward;
}

}

void RpcServer::fill_block_header_response(const BlockTemplate& blk, bool orphan_status, uint32_t index, const Hash& hash, block_header_response& response) {
	response.major_version = blk.majorVersion;
	response.minor_version = blk.minorVersion;
	response.timestamp = blk.timestamp;
	response.prev_hash = Common::podToHex(blk.previousBlockHash);
	response.nonce = blk.nonce;
	response.orphan_status = orphan_status;
	response.height = index;
	response.depth = m_core.getTopBlockIndex() - index;
	response.hash = Common::podToHex(hash);
	response.difficulty = m_core.getBlockDifficulty(index);
	response.reward = get_block_reward(blk);
	BlockDetails blkDetails = m_core.getBlockDetails(hash);
	response.num_txes = static_cast<uint32_t>(blkDetails.transactions.size());
	response.block_size = blkDetails.blockSize;
}

bool RpcServer::on_get_last_block_header(const COMMAND_RPC_GET_LAST_BLOCK_HEADER::request& req, COMMAND_RPC_GET_LAST_BLOCK_HEADER::response& res) {
  auto topBlock = m_core.getBlockByHash(m_core.getTopBlockHash());
  fill_block_header_response(topBlock, false, m_core.getTopBlockIndex(), m_core.getTopBlockHash(), res.block_header);
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_get_block_header_by_hash(const COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::request& req, COMMAND_RPC_GET_BLOCK_HEADER_BY_HASH::response& res) {
  Hash blockHash;
  if (!parse_hash256(req.hash, blockHash)) {
    throw JsonRpc::JsonRpcError{
      CORE_RPC_ERROR_CODE_WRONG_PARAM,
      "Failed to parse hex representation of block hash. Hex = " + req.hash + '.' };
  }

  if (!m_core.hasBlock(blockHash)) {
    throw JsonRpc::JsonRpcError{
      CORE_RPC_ERROR_CODE_INTERNAL_ERROR,
      "Internal error: can't get block by hash. Hash = " + req.hash + '.' };
  }

  auto block = m_core.getBlockByHash(blockHash);
  CachedBlock cachedBlock(block);
  assert(block.baseTransaction.inputs.front().type() != typeid(BaseInput));

  fill_block_header_response(block, false, cachedBlock.getBlockIndex(), cachedBlock.getBlockHash(), res.block_header);
  res.status = CORE_RPC_STATUS_OK;
  return true;
}

bool RpcServer::on_get_block_header_by_height(const COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::request& req, COMMAND_RPC_GET_BLOCK_HEADER_BY_HEIGHT::response& res) {
  if (m_core.getTopBlockIndex() < req.height) {
    throw JsonRpc::JsonRpcError{ CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT,
      std::string("To big height: ") + std::to_string(req.height) + ", current blockchain height = " + std::to_string(m_core.getTopBlockIndex()) };
  }

uint32_t index = static_cast<uint32_t>(req.height);
  auto block = m_core.getBlockByIndex(index);
  CachedBlock cachedBlock(block);
assert(cachedBlock.getBlockIndex() == req.height);
  fill_block_header_response(block, false, index, cachedBlock.getBlockHash(), res.block_header);
  res.status = CORE_RPC_STATUS_OK;
  return true;
}


bool RpcServer::on_get_block_headers_range(const COMMAND_RPC_GET_BLOCK_HEADERS_RANGE::request& req, COMMAND_RPC_GET_BLOCK_HEADERS_RANGE::response& res, JsonRpc::JsonRpcError& error_resp) {
	// TODO: change usage to jsonRpcHandlers?
	const uint64_t bc_height = m_core.get_current_blockchain_height();
	if (req.start_height > bc_height || req.end_height >= bc_height || req.start_height > req.end_height)
	{
		error_resp.code = CORE_RPC_ERROR_CODE_TOO_BIG_HEIGHT;
		error_resp.message = "Invalid start/end heights.";
		return false;
	}

	for (uint32_t h = static_cast<uint32_t>(req.start_height); h <= static_cast<uint32_t>(req.end_height); ++h) {
		Crypto::Hash block_hash = m_core.getBlockHashByIndex(h);
		CryptoNote::BlockTemplate blk = m_core.getBlockByHash(block_hash);

		res.headers.push_back(block_header_response());
		fill_block_header_response(blk, false, h, block_hash, res.headers.back());

		// TODO: Error handling like in monero?
		/*block blk;
		bool have_block = m_core.get_block_by_hash(block_hash, blk);
		if (!have_block)
		{
			error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
			error_resp.message = "Internal error: can't get block by height. Height = " + boost::lexical_cast<std::string>(h) + ". Hash = " + epee::string_tools::pod_to_hex(block_hash) + '.';
			return false;
		}
		if (blk.miner_tx.vin.size() != 1 || blk.miner_tx.vin.front().type() != typeid(txin_gen))
		{
			error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
			error_resp.message = "Internal error: coinbase transaction in the block has the wrong type";
			return false;
		}
		uint64_t block_height = boost::get<txin_gen>(blk.miner_tx.vin.front()).height;
		if (block_height != h)
		{
			error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
			error_resp.message = "Internal error: coinbase transaction in the block has the wrong height";
			return false;
		}
		res.headers.push_back(block_header_response());
		bool response_filled = fill_block_header_response(blk, false, block_height, block_hash, res.headers.back());
		if (!response_filled)
		{
			error_resp.code = CORE_RPC_ERROR_CODE_INTERNAL_ERROR;
			error_resp.message = "Internal error: can't produce valid response.";
			return false;
		}*/
	}

	res.status = CORE_RPC_STATUS_OK;
	return true;
}

}
