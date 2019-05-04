// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "CryptoNoteProtocolHandler.h"

#include <future>
#include <boost/scope_exit.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <System/Dispatcher.h>

#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "Common/CryptoNoteTools.h"
#include "CryptoNoteCore/Currency.h"
#include "P2p/LevinProtocol.h"

#include <Serialization/SerializationTools.h>

#include <Utilities/FormatTools.h>

#include <config/Ascii.h>
#include <config/CryptoNoteConfig.h>
#include <config/WalletConfig.h>

using namespace Logging;
using namespace Common;

namespace CryptoNote {

namespace {

template<class t_parametr>
bool post_notify(IP2pEndpoint& p2p, typename t_parametr::request& arg, const CryptoNoteConnectionContext& context) {
  return p2p.invoke_notify_to_peer(t_parametr::ID, LevinProtocol::encode(arg), context);
}

template<class t_parametr>
void relay_post_notify(IP2pEndpoint& p2p, typename t_parametr::request& arg, const boost::uuids::uuid* excludeConnection = nullptr) {
  p2p.externalRelayNotifyToAll(t_parametr::ID, LevinProtocol::encode(arg), excludeConnection);
}

std::vector<RawBlockLegacy> convertRawBlocksToRawBlocksLegacy(const std::vector<RawBlock>& rawBlocks) {
  std::vector<RawBlockLegacy> legacy;
  legacy.reserve(rawBlocks.size());

  for (const auto& rawBlock: rawBlocks) {
    legacy.emplace_back(RawBlockLegacy{rawBlock.block, rawBlock.transactions});
  }

  return legacy;
}

std::vector<RawBlock> convertRawBlocksLegacyToRawBlocks(const std::vector<RawBlockLegacy>& legacy) {
  std::vector<RawBlock> rawBlocks;
  rawBlocks.reserve(legacy.size());

  for (const auto& legacyBlock: legacy) {
    rawBlocks.emplace_back(RawBlock{legacyBlock.blockTemplate, legacyBlock.transactions});
  }

  return rawBlocks;
}

}

// unpack to strings to maintain protocol compatibility with older versions
static inline void serialize(RawBlockLegacy& rawBlock, ISerializer& serializer) {
  std::string block;
  std::vector<std::string> transactions;
  if (serializer.type() == ISerializer::INPUT) {
    serializer(block, "block");
    serializer(transactions, "txs");
    rawBlock.blockTemplate.reserve(block.size());
    rawBlock.transactions.reserve(transactions.size());
    std::copy(block.begin(), block.end(), std::back_inserter(rawBlock.blockTemplate));
    std::transform(transactions.begin(), transactions.end(), std::back_inserter(rawBlock.transactions), [] (const std::string& s) {
      return BinaryArray(s.begin(), s.end());
    });
  } else {
    block.reserve(rawBlock.blockTemplate.size());
    transactions.reserve(rawBlock.transactions.size());
    std::copy(rawBlock.blockTemplate.begin(), rawBlock.blockTemplate.end(), std::back_inserter(block));
    std::transform(rawBlock.transactions.begin(), rawBlock.transactions.end(), std::back_inserter(transactions), [] (BinaryArray& s) {
      return std::string(s.begin(), s.end());
    });
    serializer(block, "block");
    serializer(transactions, "txs");
  }
}

static inline void serialize(NOTIFY_NEW_BLOCK_request& request, ISerializer& s) {
  s(request.block, "b");
  s(request.current_blockchain_height, "current_blockchain_height");
  s(request.hop, "hop");
}

// unpack to strings to maintain protocol compatibility with older versions
static inline void serialize(NOTIFY_NEW_TRANSACTIONS_request& request, ISerializer& s) {
  std::vector<std::string> transactions;
  if (s.type() == ISerializer::INPUT) {
    s(transactions, "txs");
    request.txs.reserve(transactions.size());
    std::transform(transactions.begin(), transactions.end(), std::back_inserter(request.txs), [] (const std::string& s) {
      return BinaryArray(s.begin(), s.end());
    });
  }else {
    transactions.reserve(request.txs.size());
    std::transform(request.txs.begin(), request.txs.end(), std::back_inserter(transactions), [] (const BinaryArray& s) {
      return std::string(s.begin(), s.end());
    });
    s(transactions, "txs");
  }
}

static inline void serialize(NOTIFY_RESPONSE_GET_OBJECTS_request& request, ISerializer& s) {
  s(request.txs, "txs");
  s(request.blocks, "blocks");
  serializeAsBinary(request.missed_ids, "missed_ids", s);
  s(request.current_blockchain_height, "current_blockchain_height");
}

static inline void serialize(NOTIFY_NEW_LITE_BLOCK_request& request, ISerializer& s) {
  std::string blockTemplate;

  s(request.current_blockchain_height, "current_blockchain_height");
  s(request.hop, "hop");

  if (s.type() == ISerializer::INPUT) {
    s(blockTemplate, "blockTemplate");
    request.blockTemplate.reserve(blockTemplate.size());
    std::copy(blockTemplate.begin(), blockTemplate.end(), std::back_inserter(request.blockTemplate));
  } else {
    blockTemplate.reserve(request.blockTemplate.size());
    std::copy(request.blockTemplate.begin(), request.blockTemplate.end(), std::back_inserter(blockTemplate));
    s(blockTemplate, "blockTemplate");
  }
}

static inline void serialize(NOTIFY_MISSING_TXS_request& request, ISerializer& s) {
  s(request.current_blockchain_height, "current_blockchain_height");
  s(request.blockHash, "blockHash");
  serializeAsBinary(request.missing_txs, "missing_txs", s);
}

CryptoNoteProtocolHandler::CryptoNoteProtocolHandler(const Currency& currency, System::Dispatcher& dispatcher, ICore& rcore, IP2pEndpoint* p_net_layout, std::shared_ptr<Logging::ILogger> log) :
  m_dispatcher(dispatcher),
  m_currency(currency),
  m_core(rcore),
  m_p2p(p_net_layout),
  m_synchronized(false),
  m_stop(false),
  m_observedHeight(0),
  m_blockchainHeight(0),
  m_peersCount(0),
  logger(log, "protocol") {

  if (!m_p2p) {
    m_p2p = &m_p2p_stub;
  }
}

size_t CryptoNoteProtocolHandler::getPeerCount() const {
  return m_peersCount;
}

void CryptoNoteProtocolHandler::set_p2p_endpoint(IP2pEndpoint* p2p) {
  if (p2p)
    m_p2p = p2p;
  else
    m_p2p = &m_p2p_stub;
}

void CryptoNoteProtocolHandler::onConnectionOpened(CryptoNoteConnectionContext& context) {
}

void CryptoNoteProtocolHandler::onConnectionClosed(CryptoNoteConnectionContext& context) {
  bool updated = false;
  {
    std::lock_guard<std::mutex> lock(m_observedHeightMutex);
    uint64_t prevHeight = m_observedHeight;
    recalculateMaxObservedHeight(context);
    if (prevHeight != m_observedHeight) {
      updated = true;
    }
  }

  if (updated) {
    logger(TRACE) << "Observed height updated: " << m_observedHeight;
    m_observerManager.notify(&ICryptoNoteProtocolObserver::lastKnownBlockHeightUpdated, m_observedHeight);
  }

  if (context.m_state != CryptoNoteConnectionContext::state_befor_handshake) {
    m_peersCount--;
    m_observerManager.notify(&ICryptoNoteProtocolObserver::peerCountUpdated, m_peersCount.load());
  }
}

void CryptoNoteProtocolHandler::stop() {
  m_stop = true;
}

bool CryptoNoteProtocolHandler::start_sync(CryptoNoteConnectionContext& context) {
  logger(Logging::TRACE) << context << "Starting synchronization";

  if (context.m_state == CryptoNoteConnectionContext::state_synchronizing) {
    assert(context.m_needed_objects.empty());
    assert(context.m_requested_objects.empty());

    NOTIFY_REQUEST_CHAIN::request r = boost::value_initialized<NOTIFY_REQUEST_CHAIN::request>();
    r.block_ids = m_core.buildSparseChain();
    logger(Logging::TRACE) << context << "-->>NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << r.block_ids.size();
    post_notify<NOTIFY_REQUEST_CHAIN>(*m_p2p, r, context);
  }

  return true;
}

CoreStatistics CryptoNoteProtocolHandler::getStatistics() {
  return m_core.getCoreStatistics();
}

void CryptoNoteProtocolHandler::log_connections() {
  std::stringstream ss;

  ss << std::setw(25) << std::left << "Remote Host"
    << std::setw(20) << "Peer ID"
    << std::setw(25) << "Recv/Sent (inactive,sec)"
    << std::setw(25) << "State"
    << std::setw(20) << "Lifetime(seconds)" << ENDL;

  m_p2p->for_each_connection([&](const CryptoNoteConnectionContext& cntxt, uint64_t peer_id) {
    ss << std::setw(25) << std::left << std::string(cntxt.m_is_income ? "[INCOMING]" : "[OUTGOING]") +
      Common::ipAddressToString(cntxt.m_remote_ip) + ":" + std::to_string(cntxt.m_remote_port)
      << std::setw(20) << std::hex << peer_id
      // << std::setw(25) << std::to_string(cntxt.m_recv_cnt) + "(" + std::to_string(time(NULL) - cntxt.m_last_recv) + ")" + "/" + std::to_string(cntxt.m_send_cnt) + "(" + std::to_string(time(NULL) - cntxt.m_last_send) + ")"
      << std::setw(25) << get_protocol_state_string(cntxt.m_state)
      << std::setw(20) << std::to_string(time(NULL) - cntxt.m_started) << ENDL;
  });
  logger(INFO) << "Connections: " << ENDL << ss.str();
}

uint32_t CryptoNoteProtocolHandler::get_current_blockchain_height() {
  return m_core.getTopBlockIndex() + 1;
}

bool CryptoNoteProtocolHandler::process_payload_sync_data(const CORE_SYNC_DATA& hshd, CryptoNoteConnectionContext& context, bool is_initial) {
  if (context.m_state == CryptoNoteConnectionContext::state_befor_handshake && !is_initial)
    return true;

  if (context.m_state == CryptoNoteConnectionContext::state_synchronizing) {
  } else if (m_core.hasBlock(hshd.top_id)) {
    if (is_initial) {
      on_connection_synchronized();
      context.m_state = CryptoNoteConnectionContext::state_pool_sync_required;
    } else {
      context.m_state = CryptoNoteConnectionContext::state_normal;
    }
  } else {
    uint64_t currentHeight = get_current_blockchain_height();

    uint64_t remoteHeight = hshd.current_height;

    /* Find the difference between the remote and the local height */
    int64_t diff = static_cast<int64_t>(remoteHeight) - static_cast<int64_t>(currentHeight);

    /* Find out how many days behind/ahead we are from the remote height */
    uint64_t days = std::abs(diff) / (24 * 60 * 60 / m_currency.difficultyTarget());

    std::stringstream ss;

    ss << "Your " << CRYPTONOTE_NAME << " node is syncing with the network ";

    /* We're behind the remote node */
    if (diff >= 0)
    {
        ss << "(" << Utilities::get_sync_percentage(currentHeight, remoteHeight)
          << "% complete) ";

        ss << "You are " << diff << " blocks (" << days << " days) behind ";
    }
    /* We're ahead of the remote node, no need to print percentages */
    else
    {
        ss << "You are " << std::abs(diff) << " blocks (" << days << " days) ahead ";
    }

    ss << "the current peer you're connected to. Slow and steady wins the race! ";

    auto logLevel = Logging::TRACE;
    /* Log at different levels depending upon if we're ahead, behind, and if it's
      a newly formed connection */
    if (diff >= 0)
    {
        if (is_initial)
        {
            logLevel = Logging::INFO;
        }
        else
        {
            logLevel = Logging::DEBUGGING;
        }
    }
    logger(logLevel, Logging::BRIGHT_GREEN) << context << ss.str();

    logger(Logging::DEBUGGING) << "Remote top block height: " << hshd.current_height << ", id: " << hshd.top_id;
    //let the socket to send response to handshake, but request callback, to let send request data after response
    logger(Logging::TRACE) << context << "requesting synchronization";
    context.m_state = CryptoNoteConnectionContext::state_sync_required;
  }

  updateObservedHeight(hshd.current_height, context);
  context.m_remote_blockchain_height = hshd.current_height;

  if (is_initial) {
    m_peersCount++;
    m_observerManager.notify(&ICryptoNoteProtocolObserver::peerCountUpdated, m_peersCount.load());
  }

  return true;
}

bool CryptoNoteProtocolHandler::get_payload_sync_data(CORE_SYNC_DATA& hshd) {
  hshd.top_id = m_core.getTopBlockHash();
  hshd.current_height = m_core.getTopBlockIndex() + 1;
  return true;
}

template <typename Command, typename Handler>
int notifyAdaptor(const BinaryArray& reqBuf, CryptoNoteConnectionContext& ctx, Handler handler) {

  typedef typename Command::request Request;
  int command = Command::ID;

  Request req = boost::value_initialized<Request>();
  if (!LevinProtocol::decode(reqBuf, req)) {
    throw std::runtime_error("Failed to load_from_binary in command " + std::to_string(command));
  }

  return handler(command, req, ctx);
}

// Changed std::bind -> lambda, for better debugging, remove it ASAP
#define HANDLE_NOTIFY(CMD, Handler) case CMD::ID: { ret = notifyAdaptor<CMD>(in, ctx, [this](int a1, CMD::request& a2, CryptoNoteConnectionContext& a3) { return Handler(a1, a2, a3); }); break; }

int CryptoNoteProtocolHandler::handleCommand(bool is_notify, int command, const BinaryArray& in, BinaryArray& out, CryptoNoteConnectionContext& ctx, bool& handled) {
  int ret = 0;
  handled = true;

  switch (command) {
    HANDLE_NOTIFY(NOTIFY_NEW_BLOCK, handle_notify_new_block)
    HANDLE_NOTIFY(NOTIFY_NEW_TRANSACTIONS, handle_notify_new_transactions)
    HANDLE_NOTIFY(NOTIFY_REQUEST_GET_OBJECTS, handle_request_get_objects)
    HANDLE_NOTIFY(NOTIFY_RESPONSE_GET_OBJECTS, handle_response_get_objects)
    HANDLE_NOTIFY(NOTIFY_REQUEST_CHAIN, handle_request_chain)
    HANDLE_NOTIFY(NOTIFY_RESPONSE_CHAIN_ENTRY, handle_response_chain_entry)
    HANDLE_NOTIFY(NOTIFY_REQUEST_TX_POOL, handleRequestTxPool)
    HANDLE_NOTIFY(NOTIFY_NEW_LITE_BLOCK, handle_notify_new_lite_block)
    HANDLE_NOTIFY(NOTIFY_MISSING_TXS,handle_notify_missing_txs)

  default:
    handled = false;
  }

  return ret;
}

#undef HANDLE_NOTIFY

int CryptoNoteProtocolHandler::handle_notify_new_block(int command, NOTIFY_NEW_BLOCK::request& arg, CryptoNoteConnectionContext& context) {
  logger(Logging::TRACE) << context << "NOTIFY_NEW_BLOCK (hop " << arg.hop << ")";
  updateObservedHeight(arg.current_blockchain_height, context);
  context.m_remote_blockchain_height = arg.current_blockchain_height;
  if (context.m_state != CryptoNoteConnectionContext::state_normal) {
    return 1;
  }

  auto result = m_core.addBlock(RawBlock{ arg.block.blockTemplate, arg.block.transactions });
  if (result == error::AddBlockErrorCondition::BLOCK_ADDED) {
    if (result == error::AddBlockErrorCode::ADDED_TO_ALTERNATIVE_AND_SWITCHED) {
      ++arg.hop;
      //TODO: Add here announce protocol usage
      relayBlock(arg);
      // relay_block(arg, context);
      requestMissingPoolTransactions(context);
    } else if (result == error::AddBlockErrorCode::ADDED_TO_MAIN) {
      ++arg.hop;
      //TODO: Add here announce protocol usage
      relayBlock(arg);
      // relay_block(arg, context);
    } else if (result == error::AddBlockErrorCode::ADDED_TO_ALTERNATIVE) {
      logger(Logging::TRACE) << context << "Block added as alternative";
    } else {
      logger(Logging::TRACE) << context << "Block already exists";
    }
  } else if (result == error::AddBlockErrorCondition::BLOCK_REJECTED) {
    context.m_state = CryptoNoteConnectionContext::state_synchronizing;
    NOTIFY_REQUEST_CHAIN::request r = boost::value_initialized<NOTIFY_REQUEST_CHAIN::request>();
    r.block_ids = m_core.buildSparseChain();
    logger(Logging::TRACE) << context << "-->>NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << r.block_ids.size();
    post_notify<NOTIFY_REQUEST_CHAIN>(*m_p2p, r, context);
  } else {
    logger(Logging::DEBUGGING) << context << "Block verification failed, dropping connection: " << result.message();
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
  }

  return 1;
}

int CryptoNoteProtocolHandler::handle_notify_new_transactions(int command, NOTIFY_NEW_TRANSACTIONS::request& arg, CryptoNoteConnectionContext& context) {
  logger(Logging::TRACE) << context << "NOTIFY_NEW_TRANSACTIONS";

  if (context.m_state != CryptoNoteConnectionContext::state_normal)
    return 1;

  if(context.m_pending_lite_block.has_value()) {
      logger(Logging::TRACE) << context << " Pending lite block detected, handling request as missing lite block transactions response";
      return doPushLiteBlock(context.m_pending_lite_block->request, context, std::move(arg.txs));
  } else {
      const auto it = std::remove_if(arg.txs.begin(), arg.txs.end(), [this, &context](const auto &tx)
      {
            bool failed = !this->m_core.addTransactionToPool(tx);

             if (failed)
             {
                 this->logger(Logging::DEBUGGING) << context << "Tx verification failed";
             }

            return failed;
      });
      if(it != arg.txs.end())
      {
          arg.txs.erase(it, arg.txs.end());
      }

      if (arg.txs.size() > 0) {
        //TODO: add announce usage here
        relay_post_notify<NOTIFY_NEW_TRANSACTIONS>(*m_p2p, arg, &context.m_connection_id);
      }
  }

  return true;
}

int CryptoNoteProtocolHandler::handle_request_get_objects(int command, NOTIFY_REQUEST_GET_OBJECTS::request& arg, CryptoNoteConnectionContext& context) {
  logger(Logging::TRACE) << context << "NOTIFY_REQUEST_GET_OBJECTS";
  NOTIFY_RESPONSE_GET_OBJECTS::request rsp;
  //if (!m_core.handle_get_objects(arg, rsp)) {
  //  logger(Logging::ERROR) << context << "failed to handle request NOTIFY_REQUEST_GET_OBJECTS, dropping connection";
  //  context.m_state = CryptoNoteConnectionContext::state_shutdown;
  //}

  rsp.current_blockchain_height = m_core.getTopBlockIndex() + 1;
  std::vector<RawBlock> rawBlocks;
  m_core.getBlocks(arg.blocks, rawBlocks, rsp.missed_ids);
  if (!arg.txs.empty()) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << context << "NOTIFY_RESPONSE_GET_OBJECTS: request.txs.empty() != true";
  }

  rsp.blocks = convertRawBlocksToRawBlocksLegacy(rawBlocks);

  logger(Logging::TRACE) << context << "-->>NOTIFY_RESPONSE_GET_OBJECTS: blocks.size()=" << rsp.blocks.size() << ", txs.size()=" << rsp.txs.size()
    << ", rsp.m_current_blockchain_height=" << rsp.current_blockchain_height << ", missed_ids.size()=" << rsp.missed_ids.size();
  post_notify<NOTIFY_RESPONSE_GET_OBJECTS>(*m_p2p, rsp, context);
  return 1;
}

int CryptoNoteProtocolHandler::handle_response_get_objects(int command, NOTIFY_RESPONSE_GET_OBJECTS::request& arg, CryptoNoteConnectionContext& context) {
  logger(Logging::TRACE) << context << "NOTIFY_RESPONSE_GET_OBJECTS";

  if (context.m_last_response_height > arg.current_blockchain_height) {
    logger(Logging::ERROR) << context << "sent wrong NOTIFY_HAVE_OBJECTS: arg.m_current_blockchain_height=" << arg.current_blockchain_height
      << " < m_last_response_height=" << context.m_last_response_height << ", dropping connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  updateObservedHeight(arg.current_blockchain_height, context);
  context.m_remote_blockchain_height = arg.current_blockchain_height;
  std::vector<BlockTemplate> blockTemplates;
  std::vector<CachedBlock> cachedBlocks;
  blockTemplates.resize(arg.blocks.size());
  cachedBlocks.reserve(arg.blocks.size());

  std::vector<RawBlock> rawBlocks = convertRawBlocksLegacyToRawBlocks(arg.blocks);

  for (size_t index = 0; index < rawBlocks.size(); ++index) {
    if (!fromBinaryArray(blockTemplates[index], rawBlocks[index].block)) {
      logger(Logging::ERROR) << context << "sent wrong block: failed to parse and validate block: \r\n"
        << toHex(rawBlocks[index].block) << "\r\n dropping connection";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    }

    cachedBlocks.emplace_back(blockTemplates[index]);
    if (index == 1) {
      if (m_core.hasBlock(cachedBlocks.back().getBlockHash())) { //TODO
        context.m_state = CryptoNoteConnectionContext::state_idle;
        context.m_needed_objects.clear();
        context.m_requested_objects.clear();
        logger(Logging::DEBUGGING) << context << "Connection set to idle state.";
        return 1;
      }
    }

    auto req_it = context.m_requested_objects.find(cachedBlocks.back().getBlockHash());
    if (req_it == context.m_requested_objects.end()) {
      logger(Logging::ERROR) << context << "sent wrong NOTIFY_RESPONSE_GET_OBJECTS: block with id=" << Common::podToHex(cachedBlocks.back().getBlockHash())
        << " wasn't requested, dropping connection";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    }

    if (cachedBlocks.back().getBlock().transactionHashes.size() != rawBlocks[index].transactions.size()) {
      logger(Logging::ERROR) << context
        << "sent wrong NOTIFY_RESPONSE_GET_OBJECTS: block with id=" << Common::podToHex(cachedBlocks.back().getBlockHash())
        << ", transactionHashes.size()=" << cachedBlocks.back().getBlock().transactionHashes.size()
        << " mismatch with block_complete_entry.m_txs.size()=" << rawBlocks[index].transactions.size()
        << ", dropping connection";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    }

    context.m_requested_objects.erase(req_it);
  }

  if (context.m_requested_objects.size()) {
    logger(Logging::ERROR, Logging::BRIGHT_RED) << context <<
      "returned not all requested objects (context.m_requested_objects.size()="
      << context.m_requested_objects.size() << "), dropping connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  {
    int result = processObjects(context, std::move(rawBlocks), cachedBlocks);
    if (result != 0) {
      return result;
    }
  }

  logger(DEBUGGING, BRIGHT_GREEN) << "Local blockchain updated, new index = " << m_core.getTopBlockIndex();
  if (!m_stop && context.m_state == CryptoNoteConnectionContext::state_synchronizing) {
    request_missing_objects(context, true);
  }

  return 1;
}

int CryptoNoteProtocolHandler::processObjects(CryptoNoteConnectionContext& context, std::vector<RawBlock>&& rawBlocks, const std::vector<CachedBlock>& cachedBlocks) {
  assert(rawBlocks.size() == cachedBlocks.size());
  for (size_t index = 0; index < rawBlocks.size(); ++index) {
    if (m_stop) {
      break;
    }

    auto addResult = m_core.addBlock(cachedBlocks[index], std::move(rawBlocks[index]));
    if (addResult == error::AddBlockErrorCondition::BLOCK_VALIDATION_FAILED ||
        addResult == error::AddBlockErrorCondition::TRANSACTION_VALIDATION_FAILED ||
        addResult == error::AddBlockErrorCondition::DESERIALIZATION_FAILED) {
      logger(Logging::DEBUGGING) << context << "Block verification failed, dropping connection: " << addResult.message();
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    } else if (addResult == error::AddBlockErrorCondition::BLOCK_REJECTED) {
      logger(Logging::INFO) << context << "Block received at sync phase was marked as orphaned, dropping connection: " << addResult.message();
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    } else if (addResult == error::AddBlockErrorCode::ALREADY_EXISTS) {
      logger(Logging::DEBUGGING) << context << "Block already exists, switching to idle state: " << addResult.message();
      context.m_state = CryptoNoteConnectionContext::state_idle;
      context.m_needed_objects.clear();
      context.m_requested_objects.clear();
      return 1;
    }

    m_dispatcher.yield();
  }

  return 0;
}

int CryptoNoteProtocolHandler::doPushLiteBlock(NOTIFY_NEW_LITE_BLOCK::request arg, CryptoNoteConnectionContext &context, std::vector<BinaryArray> missingTxs)
{
    BlockTemplate newBlockTemplate;
    if(!fromBinaryArray(newBlockTemplate, arg.blockTemplate)) { // deserialize blockTemplate
        logger(Logging::WARNING) << context << "Deserialization of Block Template failed, dropping connection" ;
        context.m_state = CryptoNoteConnectionContext::state_shutdown;
        return 1;
    }

    std::unordered_map<Crypto::Hash, BinaryArray> provided_txs;
    provided_txs.reserve(missingTxs.size());
    for(const auto& iMissingTx : missingTxs) {
        CachedTransaction i_provided_transaction{iMissingTx};
        provided_txs[getBinaryArrayHash(iMissingTx)] = iMissingTx;
    }

    std::vector<BinaryArray> have_txs;
    std::vector<Crypto::Hash> need_txs;

    if(context.m_pending_lite_block.has_value()) {
        for(const auto& requestedTxHash : context.m_pending_lite_block->missed_transactions) {
            if(provided_txs.find(requestedTxHash) == provided_txs.end()) {
                logger(Logging::DEBUGGING) << context << "Peer didn't provide a missing transaction, previously "
                                                         "acquired for a lite block, dropping connection.";
                context.m_pending_lite_block = std::nullopt;
                context.m_state = CryptoNoteConnectionContext::state_shutdown;
                return 1;
            }
        }
    }

    /*
   * here we are finding out which txs are
   * present in the pool and which are not
   * further we check for transactions in
   * the blockchain to accept alternative
   * blocks.
   */
    for (const auto& transactionHash: newBlockTemplate.transactionHashes) {
        auto providedSearch = provided_txs.find(transactionHash);
        if(providedSearch != provided_txs.end()) {
            have_txs.push_back(providedSearch->second);
        }
        else {
            const auto transactionBlob = m_core.getTransaction(transactionHash);
            if (transactionBlob.has_value()) {
                have_txs.push_back(*transactionBlob);
            } else {
                need_txs.push_back(transactionHash);
            }
        }
    }

    /*
   * if all txs are present then continue adding the
   * block to DB and relaying the lite-block to other peers
   *
   * if not request the missing txs from the sender
   * of the lite-block request
   */
    if (need_txs.empty()) {
        context.m_pending_lite_block = std::nullopt;
        auto result = m_core.addBlock(RawBlock{arg.blockTemplate, have_txs});
        if (result == error::AddBlockErrorCondition::BLOCK_ADDED) {
            if (result == error::AddBlockErrorCode::ADDED_TO_ALTERNATIVE_AND_SWITCHED) {
                ++arg.hop;
                //TODO: Add here announce protocol usage
                relay_post_notify<NOTIFY_NEW_LITE_BLOCK>(*m_p2p, arg, &context.m_connection_id);
                // relay_block(arg, context);
                requestMissingPoolTransactions(context);
            } else if (result == error::AddBlockErrorCode::ADDED_TO_MAIN) {
                ++arg.hop;
                //TODO: Add here announce protocol usage
                relay_post_notify<NOTIFY_NEW_LITE_BLOCK>(*m_p2p, arg, &context.m_connection_id);
                // relay_block(arg, context);
            } else if (result == error::AddBlockErrorCode::ADDED_TO_ALTERNATIVE) {
                logger(Logging::TRACE) << context << "Block added as alternative";
            } else {
                logger(Logging::TRACE) << context << "Block already exists";
            }
        } else if (result == error::AddBlockErrorCondition::BLOCK_REJECTED) {
            context.m_state = CryptoNoteConnectionContext::state_synchronizing;
            NOTIFY_REQUEST_CHAIN::request r = boost::value_initialized<NOTIFY_REQUEST_CHAIN::request>();
            r.block_ids = m_core.buildSparseChain();
            logger(Logging::TRACE) << context << "-->>NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << r.block_ids.size();
            post_notify<NOTIFY_REQUEST_CHAIN>(*m_p2p, r, context);
        } else {
            logger(Logging::DEBUGGING) << context << "Block verification failed, dropping connection: " << result.message();
            context.m_state = CryptoNoteConnectionContext::state_shutdown;
        }
    }
    else {
        if(context.m_pending_lite_block.has_value()) {
            context.m_pending_lite_block = std::nullopt;
            logger(Logging::DEBUGGING) << context << " Peer has a pending lite block but didn't provide all necessary transactions, dropping the connection.";
            context.m_state= CryptoNoteConnectionContext::state_shutdown;
        }
        else {
            NOTIFY_MISSING_TXS::request req;
            req.current_blockchain_height = arg.current_blockchain_height;
            req.blockHash = CachedBlock(newBlockTemplate).getBlockHash();
            req.missing_txs = std::move(need_txs);
            context.m_pending_lite_block = PendingLiteBlock{arg, {req.missing_txs.begin(), req.missing_txs.end()}};

            if (!post_notify<NOTIFY_MISSING_TXS>(*m_p2p, req, context)) {
                logger(Logging::DEBUGGING)
                << context << "Lite block is missing transactions but the publisher is not reachable, dropping connection.";
                context.m_state = CryptoNoteConnectionContext::state_shutdown;
            }
        }
    }

    return 1;
}

int CryptoNoteProtocolHandler::handle_request_chain(int command, NOTIFY_REQUEST_CHAIN::request& arg, CryptoNoteConnectionContext& context) {
  logger(Logging::TRACE) << context << "NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << arg.block_ids.size();

  if (arg.block_ids.empty()) {
    logger(Logging::DEBUGGING, Logging::BRIGHT_RED) << context << "Failed to handle NOTIFY_REQUEST_CHAIN. block_ids is empty";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  if (arg.block_ids.back() != m_core.getBlockHashByIndex(0)) {
    logger(Logging::DEBUGGING) << context << "Failed to handle NOTIFY_REQUEST_CHAIN. block_ids doesn't end with genesis block ID";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  NOTIFY_RESPONSE_CHAIN_ENTRY::request r;
  r.m_block_ids = m_core.findBlockchainSupplement(arg.block_ids, BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT, r.total_height, r.start_height);

  logger(Logging::TRACE) << context << "-->>NOTIFY_RESPONSE_CHAIN_ENTRY: m_start_height=" << r.start_height << ", m_total_height=" << r.total_height << ", m_block_ids.size()=" << r.m_block_ids.size();
  post_notify<NOTIFY_RESPONSE_CHAIN_ENTRY>(*m_p2p, r, context);
  return 1;
}

bool CryptoNoteProtocolHandler::request_missing_objects(CryptoNoteConnectionContext& context, bool check_having_blocks) {
  if (context.m_needed_objects.size()) {
    //we know objects that we need, request this objects
    NOTIFY_REQUEST_GET_OBJECTS::request req;
    size_t count = 0;
    auto it = context.m_needed_objects.begin();

    while (it != context.m_needed_objects.end() && count < BLOCKS_SYNCHRONIZING_DEFAULT_COUNT) {
      if (!(check_having_blocks && m_core.hasBlock(*it))) {
        req.blocks.push_back(*it);
        ++count;
        context.m_requested_objects.insert(*it);
      }
      it = context.m_needed_objects.erase(it);
    }
    logger(Logging::TRACE) << context << "-->>NOTIFY_REQUEST_GET_OBJECTS: blocks.size()=" << req.blocks.size() << ", txs.size()=" << req.txs.size();
    post_notify<NOTIFY_REQUEST_GET_OBJECTS>(*m_p2p, req, context);
  } else if (context.m_last_response_height < context.m_remote_blockchain_height - 1) {//we have to fetch more objects ids, request blockchain entry

    NOTIFY_REQUEST_CHAIN::request r = boost::value_initialized<NOTIFY_REQUEST_CHAIN::request>();
    r.block_ids = m_core.buildSparseChain();
    logger(Logging::TRACE) << context << "-->>NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << r.block_ids.size();
    post_notify<NOTIFY_REQUEST_CHAIN>(*m_p2p, r, context);
  } else {
    if (!(context.m_last_response_height ==
      context.m_remote_blockchain_height - 1 &&
      !context.m_needed_objects.size() &&
      !context.m_requested_objects.size())) {
      logger(Logging::ERROR, Logging::BRIGHT_RED)
        << "request_missing_blocks final condition failed!"
        << "\r\nm_last_response_height=" << context.m_last_response_height
        << "\r\nm_remote_blockchain_height=" << context.m_remote_blockchain_height
        << "\r\nm_needed_objects.size()=" << context.m_needed_objects.size()
        << "\r\nm_requested_objects.size()=" << context.m_requested_objects.size()
        << "\r\non connection [" << context << "]";
      return false;
    }

    requestMissingPoolTransactions(context);

    context.m_state = CryptoNoteConnectionContext::state_normal;
    logger(Logging::INFO, Logging::BRIGHT_GREEN) << context << "Successfully synchronized with the "
                                                 << CryptoNote::CRYPTONOTE_NAME << " Network.";
    on_connection_synchronized();
  }
  return true;
}

bool CryptoNoteProtocolHandler::on_connection_synchronized() {
  bool val_expected = false;
  if (m_synchronized.compare_exchange_strong(val_expected, true)) {
    logger(Logging::INFO)
      << ENDL ;
      logger(INFO, BRIGHT_MAGENTA) << "===[ " + std::string(CryptoNote::CRYPTONOTE_NAME) + " Tip! ]=============================" << ENDL ;
      logger(INFO, WHITE) << " Always exit " + WalletConfig::daemonName + " and " + WalletConfig::walletName + " with the \"exit\" command to preserve your chain and wallet data." << ENDL ;
      logger(INFO, WHITE) << " Use the \"help\" command to see a list of available commands." << ENDL ;
      logger(INFO, WHITE) << " Use the \"backup\" command in " + WalletConfig::walletName + " to display your keys/seed for restoring a corrupted wallet." << ENDL ;
      logger(INFO, WHITE) << " If you need more assistance, you can contact us for support at " + WalletConfig::contactLink << ENDL;
      logger(INFO, BRIGHT_MAGENTA) << "===================================================" << ENDL << ENDL ;

      logger(INFO, BRIGHT_GREEN) << asciiArt << ENDL;

    m_observerManager.notify(&ICryptoNoteProtocolObserver::blockchainSynchronized, m_core.getTopBlockIndex());
  }
  return true;
}

int CryptoNoteProtocolHandler::handle_response_chain_entry(int command, NOTIFY_RESPONSE_CHAIN_ENTRY::request& arg, CryptoNoteConnectionContext& context) {
  logger(Logging::TRACE) << context << "NOTIFY_RESPONSE_CHAIN_ENTRY: m_block_ids.size()=" << arg.m_block_ids.size()
    << ", m_start_height=" << arg.start_height << ", m_total_height=" << arg.total_height;

  if (!arg.m_block_ids.size()) {
    logger(Logging::ERROR) << context << "sent empty m_block_ids, dropping connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  if (!m_core.hasBlock(arg.m_block_ids.front())) {
    logger(Logging::ERROR)
      << context << "sent m_block_ids starting from unknown id: "
      << Common::podToHex(arg.m_block_ids.front())
      << " , dropping connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }

  context.m_remote_blockchain_height = arg.total_height;
  context.m_last_response_height = arg.start_height + static_cast<uint32_t>(arg.m_block_ids.size()) - 1;

  if (context.m_last_response_height > context.m_remote_blockchain_height) {
    logger(Logging::ERROR)
      << context
      << "sent wrong NOTIFY_RESPONSE_CHAIN_ENTRY, with \r\nm_total_height="
      << arg.total_height << "\r\nm_start_height=" << arg.start_height
      << "\r\nm_block_ids.size()=" << arg.m_block_ids.size();
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
  }

  bool allBlocksKnown = true;
  for (auto& bl_id : arg.m_block_ids) {
    if (allBlocksKnown) {
      if (!m_core.hasBlock(bl_id)) {
        context.m_needed_objects.push_back(bl_id);
        allBlocksKnown = false;
      }
    } else {
      context.m_needed_objects.push_back(bl_id);
    }
  }

  request_missing_objects(context, false);
  return 1;
}

int CryptoNoteProtocolHandler::handleRequestTxPool(int command, NOTIFY_REQUEST_TX_POOL::request& arg,
                                                     CryptoNoteConnectionContext& context) {
  logger(Logging::TRACE) << context << "NOTIFY_REQUEST_TX_POOL: txs.size() = " << arg.txs.size();
  NOTIFY_NEW_TRANSACTIONS::request notification;
  std::vector<Crypto::Hash> deletedTransactions;
  m_core.getPoolChanges(m_core.getTopBlockHash(), arg.txs, notification.txs, deletedTransactions);
  if (!notification.txs.empty()) {
    bool ok = post_notify<NOTIFY_NEW_TRANSACTIONS>(*m_p2p, notification, context);
    if (!ok) {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Failed to post notification NOTIFY_NEW_TRANSACTIONS to " << context.m_connection_id;
    }
  }

  return 1;
}

int CryptoNoteProtocolHandler::handle_notify_new_lite_block(int command, NOTIFY_NEW_LITE_BLOCK::request& arg,
                                                     CryptoNoteConnectionContext& context) {
  logger(Logging::TRACE) << context << "NOTIFY_NEW_LITE_BLOCK (hop " << arg.hop << ")";
  updateObservedHeight(arg.current_blockchain_height, context);
  context.m_remote_blockchain_height = arg.current_blockchain_height;
  if (context.m_state != CryptoNoteConnectionContext::state_normal) {
    return 1;
  }

  return doPushLiteBlock(std::move(arg), context, {});
}

int CryptoNoteProtocolHandler::handle_notify_missing_txs(int command, NOTIFY_MISSING_TXS::request& arg,
                                                     CryptoNoteConnectionContext& context) {
  logger(Logging::TRACE) << context << "NOTIFY_MISSING_TXS";

  NOTIFY_NEW_TRANSACTIONS::request req;

  std::vector<BinaryArray> txs;
  std::vector<Crypto::Hash> missedHashes;
  m_core.getTransactions(arg.missing_txs, txs, missedHashes);
  if (!missedHashes.empty()) {
    logger(Logging::DEBUGGING) << "Failed to Handle NOTIFY_MISSING_TXS, Unable to retrieve requested transactions, Dropping Connection";
    context.m_state = CryptoNoteConnectionContext::state_shutdown;
    return 1;
  }
  else {
    req.txs = std::move(txs);
  }

  logger(Logging::DEBUGGING) << "--> NOTIFY_RESPONSE_MISSING_TXS: "
                             << "txs.size() = " << req.txs.size();

  if (post_notify<NOTIFY_NEW_TRANSACTIONS>(*m_p2p, req, context)) {
    logger(Logging::DEBUGGING) << "NOTIFY_MISSING_TXS response sent to peer successfully";
  }
  else {
    logger(Logging::DEBUGGING) << "Error while sending NOTIFY_MISSING_TXS response to peer";
  }

  return 1;
}

void CryptoNoteProtocolHandler::relayBlock(NOTIFY_NEW_BLOCK::request& arg) {

  // generate a lite block request from the received normal block.
  NOTIFY_NEW_LITE_BLOCK::request lite_arg;
  lite_arg.current_blockchain_height = arg.current_blockchain_height;
  lite_arg.blockTemplate = arg.block.blockTemplate;
  lite_arg.hop = arg.hop;

  // encoding the request for sending the blocks to peers.
  auto buf = LevinProtocol::encode(arg);
  auto lite_buf = LevinProtocol::encode(lite_arg);

  // logging the msg size to see the difference in payload size.
  logger(Logging::DEBUGGING) << "NOTIFY_NEW_BLOCK - MSG_SIZE = " << buf.size();
  logger(Logging::DEBUGGING) << "NOTIFY_NEW_LITE_BLOCK - MSG_SIZE = " << lite_buf.size();

  std::list<boost::uuids::uuid> liteBlockConnections, normalBlockConnections;

  // sort the peers into their support categories.
  m_p2p->for_each_connection([this, &liteBlockConnections, &normalBlockConnections](const CryptoNoteConnectionContext& ctx, uint64_t peerId){
    if (ctx.version >= P2P_LITE_BLOCKS_PROPOGATION_VERSION) {
      logger(Logging::DEBUGGING) << ctx << "Peer supports lite-blocks... adding peer to lite block list";
      liteBlockConnections.push_back(ctx.m_connection_id);
    }
    else {
      logger(Logging::DEBUGGING) << ctx << "Peer doesn't support lite-blocks... adding peer to normal block list";
      normalBlockConnections.push_back(ctx.m_connection_id);
    }
  });

  // first send lite one's.. coz they are faster
  if(!liteBlockConnections.empty()) {
    m_p2p->externalRelayNotifyToList(NOTIFY_NEW_LITE_BLOCK::ID, lite_buf, liteBlockConnections);
  }

  if (!normalBlockConnections.empty()) {
    m_p2p->externalRelayNotifyToList(NOTIFY_NEW_BLOCK::ID, buf, normalBlockConnections);
  }
}

void CryptoNoteProtocolHandler::relayTransactions(const std::vector<BinaryArray>& transactions) {
  auto buf = LevinProtocol::encode(NOTIFY_NEW_TRANSACTIONS::request{transactions});
  m_p2p->externalRelayNotifyToAll(NOTIFY_NEW_TRANSACTIONS::ID, buf, nullptr);
}

void CryptoNoteProtocolHandler::requestMissingPoolTransactions(const CryptoNoteConnectionContext& context) {
  if (context.version < 1) {
    return;
  }

  NOTIFY_REQUEST_TX_POOL::request notification;
  notification.txs = m_core.getPoolTransactionHashes();

  bool ok = post_notify<NOTIFY_REQUEST_TX_POOL>(*m_p2p, notification, context);
  if (!ok) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Failed to post notification NOTIFY_REQUEST_TX_POOL to " << context.m_connection_id;
  }
}

void CryptoNoteProtocolHandler::updateObservedHeight(uint32_t peerHeight, const CryptoNoteConnectionContext& context) {
  bool updated = false;
  {
    std::lock_guard<std::mutex> lock(m_observedHeightMutex);

    uint32_t height = m_observedHeight;
    if (context.m_remote_blockchain_height != 0 && context.m_last_response_height <= context.m_remote_blockchain_height - 1) {
      m_observedHeight = context.m_remote_blockchain_height - 1;
      if (m_observedHeight != height) {
        updated = true;
      }
    } else if (peerHeight > context.m_remote_blockchain_height) {
      m_observedHeight = std::max(m_observedHeight, peerHeight);
      if (m_observedHeight != height) {
        updated = true;
      }
    } else if (peerHeight != context.m_remote_blockchain_height && context.m_remote_blockchain_height == m_observedHeight) {
      //the client switched to alternative chain and had maximum observed height. need to recalculate max height
      recalculateMaxObservedHeight(context);
      if (m_observedHeight != height) {
        updated = true;
      }
    }
  }

  {
    std::lock_guard<std::mutex> lock(m_blockchainHeightMutex);
    if (peerHeight > m_blockchainHeight) {
      m_blockchainHeight = peerHeight;
      logger(Logging::INFO, Logging::BRIGHT_GREEN) << "New Top Block Detected: " << peerHeight;
    }
  }


  if (updated) {
    logger(TRACE) << "Observed height updated: " << m_observedHeight;
    m_observerManager.notify(&ICryptoNoteProtocolObserver::lastKnownBlockHeightUpdated, m_observedHeight);
  }
}

void CryptoNoteProtocolHandler::recalculateMaxObservedHeight(const CryptoNoteConnectionContext& context) {
  //should be locked outside
  uint32_t peerHeight = 0;
  m_p2p->for_each_connection([&peerHeight, &context](const CryptoNoteConnectionContext& ctx, uint64_t peerId) {
    if (ctx.m_connection_id != context.m_connection_id) {
      peerHeight = std::max(peerHeight, ctx.m_remote_blockchain_height);
    }
  });

  m_observedHeight = std::max(peerHeight, m_core.getTopBlockIndex() + 1);
    if (context.m_state == CryptoNoteConnectionContext::state_normal) {
      m_observedHeight = m_core.getTopBlockIndex();
    }
}

uint32_t CryptoNoteProtocolHandler::getObservedHeight() const {
  std::lock_guard<std::mutex> lock(m_observedHeightMutex);
  return m_observedHeight;
};

uint32_t CryptoNoteProtocolHandler::getBlockchainHeight() const {
  std::lock_guard<std::mutex> lock(m_blockchainHeightMutex);
  return m_blockchainHeight;
};

bool CryptoNoteProtocolHandler::addObserver(ICryptoNoteProtocolObserver* observer) {
  return m_observerManager.add(observer);
}

bool CryptoNoteProtocolHandler::removeObserver(ICryptoNoteProtocolObserver* observer) {
  return m_observerManager.remove(observer);
}

};
