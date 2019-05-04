// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2019, The CyprusCoin Developers
//
// Please see the included LICENSE file for more information.

#include "NetNode.h"

#include <algorithm>
#include <fstream>

#include <boost/foreach.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/utility/value_init.hpp>

#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>

#include <System/Context.h>
#include <System/ContextGroupTimeout.h>
#include <System/EventLock.h>
#include <System/InterruptedException.h>
#include <System/Ipv4Address.h>
#include <System/Ipv4Resolver.h>
#include <System/TcpListener.h>
#include <System/TcpConnector.h>

#include "version.h"
#include <config/CryptoNoteConfig.h>
#include "Common/StdInputStream.h"
#include "Common/StdOutputStream.h"
#include "Common/Util.h"
#include "crypto/crypto.h"
#include <crypto/random.h>

#include "ConnectionContext.h"
#include "LevinProtocol.h"
#include "P2pProtocolDefinitions.h"

#include "Serialization/BinaryInputStreamSerializer.h"
#include "Serialization/BinaryOutputStreamSerializer.h"
#include "Serialization/SerializationOverloads.h"

using namespace Common;
using namespace Logging;
using namespace CryptoNote;

namespace {

size_t get_random_index_with_fixed_probability(size_t max_index) {
  //divide by zero workaround
  if (!max_index)
    return 0;
  size_t x = Random::randomValue<size_t>() % (max_index + 1);
  return (x*x*x) / (max_index*max_index); //parabola \/
}


void addPortMapping(Logging::LoggerRef& logger, uint32_t port) {
  // Add UPnP port mapping
  logger(INFO) << "Attempting to add IGD port mapping.";
  int result;
  UPNPDev* deviceList = upnpDiscover(1000, NULL, NULL, 0, 0, 2, &result);
  UPNPUrls urls;
  IGDdatas igdData;
  char lanAddress[64];
  result = UPNP_GetValidIGD(deviceList, &urls, &igdData, lanAddress, sizeof lanAddress);
  freeUPNPDevlist(deviceList);
  if (result != 0) {
    if (result == 1) {
      std::ostringstream portString;
      portString << port;
      if (UPNP_AddPortMapping(urls.controlURL, igdData.first.servicetype, portString.str().c_str(),
        portString.str().c_str(), lanAddress, CryptoNote::CRYPTONOTE_NAME, "TCP", 0, "0") != 0) {
        logger(ERROR) << "UPNP_AddPortMapping failed.";
      } else {
        logger(INFO) << "Added IGD port mapping.";
      }
    } else if (result == 2) {
      logger(INFO) << "IGD was found but reported as not connected.";
    } else if (result == 3) {
      logger(INFO) << "UPnP device was found but not recognized as IGD.";
    } else {
      logger(ERROR) << "UPNP_GetValidIGD returned an unknown result code.";
    }

    FreeUPNPUrls(&urls);
  } else {
    logger(INFO) << "No IGD was found.";
  }
}

}


namespace CryptoNote {
namespace {

std::string print_peerlist_to_string(const std::list<PeerlistEntry>& pl) {
  time_t now_time = 0;
  time(&now_time);
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(8) << std::hex << std::noshowbase;
  for (const auto& pe : pl) {
    ss << pe.id << "\t" << pe.adr << " \tlast_seen: " << Common::timeIntervalToString(now_time - pe.last_seen) << std::endl;
  }
  return ss.str();
}

}


  //-----------------------------------------------------------------------------------
  // P2pConnectionContext implementation
  //-----------------------------------------------------------------------------------

  bool P2pConnectionContext::pushMessage(P2pMessage&& msg) {
    writeQueueSize += msg.size();

    if (writeQueueSize > P2P_CONNECTION_MAX_WRITE_BUFFER_SIZE) {
      logger(DEBUGGING) << *this << "Write queue overflows. Interrupt connection";
      interrupt();
      return false;
    }

    writeQueue.push_back(std::move(msg));
    queueEvent.set();
    return true;
  }

  std::vector<P2pMessage> P2pConnectionContext::popBuffer() {
    writeOperationStartTime = TimePoint();

    while (writeQueue.empty() && !stopped) {
      queueEvent.wait();
    }

    std::vector<P2pMessage> msgs(std::move(writeQueue));
    writeQueue.clear();
    writeQueueSize = 0;
    writeOperationStartTime = Clock::now();
    queueEvent.clear();
    return msgs;
  }

  uint64_t P2pConnectionContext::writeDuration(TimePoint now) const { // in milliseconds
    return writeOperationStartTime == TimePoint() ? 0 : std::chrono::duration_cast<std::chrono::milliseconds>(now - writeOperationStartTime).count();
  }

  void P2pConnectionContext::interrupt() {
    logger(DEBUGGING) << *this << "Interrupt connection";
    assert(context != nullptr);
    stopped = true;
    queueEvent.set();
    context->interrupt();
  }


  template <typename Command, typename Handler>
  int invokeAdaptor(const BinaryArray& reqBuf, BinaryArray& resBuf, P2pConnectionContext& ctx, Handler handler) {
    typedef typename Command::request Request;
    typedef typename Command::response Response;
    int command = Command::ID;

    Request req = boost::value_initialized<Request>();

    if (!LevinProtocol::decode(reqBuf, req)) {
      throw std::runtime_error("Failed to load_from_binary in command " + std::to_string(command));
    }

    Response res = boost::value_initialized<Response>();
    int ret = handler(command, req, res, ctx);
    resBuf = LevinProtocol::encode(res);
    return ret;
  }

  NodeServer::NodeServer(System::Dispatcher& dispatcher, CryptoNote::CryptoNoteProtocolHandler& payload_handler, std::shared_ptr<Logging::ILogger> log) :
    m_dispatcher(dispatcher),
    m_workingContextGroup(dispatcher),
    m_payload_handler(payload_handler),
    m_allow_local_ip(false),
    m_hide_my_port(false),
    m_network_id(CryptoNote::CRYPTONOTE_NETWORK),
    logger(log, "node_server"),
    m_stopEvent(m_dispatcher),
    m_idleTimer(m_dispatcher),
    m_timedSyncTimer(m_dispatcher),
    m_timeoutTimer(m_dispatcher),
    m_stop(false),
    // intervals
    // m_peer_handshake_idle_maker_interval(CryptoNote::P2P_DEFAULT_HANDSHAKE_INTERVAL),
    m_connections_maker_interval(1),
    m_peerlist_store_interval(60*30, false) {
  }

  void NodeServer::serialize(ISerializer& s) {
    uint8_t version = 1;
    s(version, "version");

    if (version != 1) {
      throw std::runtime_error("Unsupported version");
    }

    s(m_peerlist, "peerlist");
    s(m_config.m_peer_id, "peer_id");
  }

  using namespace std::placeholders;

  #define INVOKE_HANDLER(CMD, Handler) case CMD::ID: { ret = invokeAdaptor<CMD>(cmd.buf, out, ctx,  std::bind(Handler, this, _1, _2, _3, _4)); break; }

  int NodeServer::handleCommand(const LevinProtocol::Command& cmd, BinaryArray& out, P2pConnectionContext& ctx, bool& handled) {
    int ret = 0;
    handled = true;

    if (cmd.isResponse && cmd.command == COMMAND_TIMED_SYNC::ID) {
      if (!handleTimedSyncResponse(cmd.buf, ctx)) {
        // invalid response, close connection
        ctx.m_state = CryptoNoteConnectionContext::state_shutdown;
      }
      return 0;
    }

    switch (cmd.command) {
      INVOKE_HANDLER(COMMAND_HANDSHAKE, &NodeServer::handle_handshake)
      INVOKE_HANDLER(COMMAND_TIMED_SYNC, &NodeServer::handle_timed_sync)
      INVOKE_HANDLER(COMMAND_PING, &NodeServer::handle_ping)
#ifdef ALLOW_DEBUG_COMMANDS
      INVOKE_HANDLER(COMMAND_REQUEST_STAT_INFO, &NodeServer::handle_get_stat_info)
      INVOKE_HANDLER(COMMAND_REQUEST_NETWORK_STATE, &NodeServer::handle_get_network_state)
      INVOKE_HANDLER(COMMAND_REQUEST_PEER_ID, &NodeServer::handle_get_peer_id)
#endif
    default: {
        handled = false;
        ret = m_payload_handler.handleCommand(cmd.isNotify, cmd.command, cmd.buf, out, ctx, handled);
      }
    }

    return ret;
  }

#undef INVOKE_HANDLER

  bool NodeServer::init_config() {
    try {
      std::string state_file_path = m_config_folder + "/" + m_p2p_state_filename;
      bool loaded = false;

      try {
        std::ifstream p2p_data;

        std::ios_base::openmode open_mode = std::ios_base::binary | std::ios_base::in;
        /* --p2p-reset-peerstate daemon option 
          Truncates the file if want the peer state reset */
        if(m_p2p_state_reset) {
          open_mode |= std::ios_base::trunc;
        }
        p2p_data.open(state_file_path, open_mode);

        if (!p2p_data.fail()) {
          StdInputStream inputStream(p2p_data);
          BinaryInputStreamSerializer a(inputStream);
          CryptoNote::serialize(*this, a);
          loaded = true;
        }
      } catch (const std::exception& e) {
        logger(ERROR, BRIGHT_RED) << "Failed to load config from file '" << state_file_path << "': " << e.what();
      }

      if (!loaded) {
        make_default_config();
      }

      //at this moment we have hardcoded config
      m_config.m_net_config.handshake_interval = CryptoNote::P2P_DEFAULT_HANDSHAKE_INTERVAL;
      m_config.m_net_config.connections_count = CryptoNote::P2P_DEFAULT_CONNECTIONS_COUNT;
      m_config.m_net_config.packet_max_size = CryptoNote::P2P_DEFAULT_PACKET_MAX_SIZE; //20 MB limit
      m_config.m_net_config.config_id = 0; // initial config
      m_config.m_net_config.connection_timeout = CryptoNote::P2P_DEFAULT_CONNECTION_TIMEOUT;
      m_config.m_net_config.ping_connection_timeout = CryptoNote::P2P_DEFAULT_PING_CONNECTION_TIMEOUT;
      m_config.m_net_config.send_peerlist_sz = CryptoNote::P2P_DEFAULT_PEERS_IN_HANDSHAKE;

      m_first_connection_maker_call = true;
    } catch (const std::exception& e) {
      logger(ERROR, BRIGHT_RED) << "init_config failed: " << e.what();
      return false;
    }
    return true;
  }

  //-----------------------------------------------------------------------------------
  void NodeServer::for_each_connection(std::function<void(CryptoNoteConnectionContext&, uint64_t)> f)
  {
    for (auto& ctx : m_connections) {
      f(ctx.second, ctx.second.peerId);
    }
  }

  //-----------------------------------------------------------------------------------
  void NodeServer::externalRelayNotifyToAll(int command, const BinaryArray& data_buff, const boost::uuids::uuid* excludeConnection) {
    m_dispatcher.remoteSpawn([this, command, data_buff, excludeConnection] {
      relay_notify_to_all(command, data_buff, excludeConnection);
    });
  }

  //-----------------------------------------------------------------------------------
  void NodeServer::externalRelayNotifyToList(int command, const BinaryArray& data_buff, const std::list<boost::uuids::uuid> relayList) {
    m_dispatcher.remoteSpawn([this, command, data_buff, relayList] {
      forEachConnection([&](P2pConnectionContext& conn) {
        if (std::find(relayList.begin(), relayList.end(), conn.m_connection_id) != relayList.end()) {
          if (conn.peerId && (conn.m_state == CryptoNoteConnectionContext::state_normal ||
               conn.m_state == CryptoNoteConnectionContext::state_synchronizing)) {
            conn.pushMessage(P2pMessage(P2pMessage::NOTIFY, command, data_buff));
          }
        }
      });
    });
  }
  //-----------------------------------------------------------------------------------
  bool NodeServer::make_default_config()
  {
    m_config.m_peer_id = Random::randomValue<uint64_t>();
    logger(INFO, BRIGHT_WHITE) << "Generated new peer ID: " << m_config.m_peer_id;
    return true;
  }

  //-----------------------------------------------------------------------------------

  bool NodeServer::handleConfig(const NetNodeConfig& config) {
    m_bind_ip = config.getBindIp();
    m_port = std::to_string(config.getBindPort());
    m_external_port = config.getExternalPort();
    m_allow_local_ip = config.getAllowLocalIp();

    auto peers = config.getPeers();
    std::copy(peers.begin(), peers.end(), std::back_inserter(m_command_line_peers));

    auto exclusiveNodes = config.getExclusiveNodes();
    std::copy(exclusiveNodes.begin(), exclusiveNodes.end(), std::back_inserter(m_exclusive_peers));

    auto priorityNodes = config.getPriorityNodes();
    std::copy(priorityNodes.begin(), priorityNodes.end(), std::back_inserter(m_priority_peers));

    auto seedNodes = config.getSeedNodes();
    std::copy(seedNodes.begin(), seedNodes.end(), std::back_inserter(m_seed_nodes));

    m_hide_my_port = config.getHideMyPort();
    return true;
  }

  bool NodeServer::append_net_address(std::vector<NetworkAddress>& nodes, const std::string& addr) {
    size_t pos = addr.find_last_of(':');
    if (!(std::string::npos != pos && addr.length() - 1 != pos && 0 != pos)) {
      logger(ERROR, BRIGHT_RED) << "Failed to parse seed address from string: '" << addr << '\'';
      return false;
    }

    std::string host = addr.substr(0, pos);

    try {
      uint32_t port = Common::fromString<uint32_t>(addr.substr(pos + 1));

      System::Ipv4Resolver resolver(m_dispatcher);
      auto addr = resolver.resolve(host);
      nodes.push_back(NetworkAddress{hostToNetwork(addr.getValue()), port});

      logger(TRACE) << "Added seed node: " << nodes.back() << " (" << host << ")";

    } catch (const std::exception& e) {
      logger(ERROR, BRIGHT_YELLOW) << "Failed to resolve host name '" << host << "': " << e.what();
      return false;
    }

    return true;
  }


  //-----------------------------------------------------------------------------------

  bool NodeServer::init(const NetNodeConfig& config) {
    if (!config.getTestnet()) {
      for (auto seed : CryptoNote::SEED_NODES) {
        append_net_address(m_seed_nodes, seed);
      }
    } else {
      m_network_id.data[0] += 1;
    }

    if (!handleConfig(config)) {
      logger(ERROR, BRIGHT_RED) << "Failed to handle command line";
      return false;
    }
    m_config_folder = config.getConfigFolder();
    m_p2p_state_filename = config.getP2pStateFilename();
    m_p2p_state_reset = config.getP2pStateReset();

    if (!init_config()) {
      logger(ERROR, BRIGHT_RED) << "Failed to init config.";
      return false;
    }

    if (!m_peerlist.init(m_allow_local_ip)) {
      logger(ERROR, BRIGHT_RED) << "Failed to init peerlist.";
      return false;
    }

    for(auto& p: m_command_line_peers)
      m_peerlist.append_with_peer_white(p);

    //only in case if we really sure that we have external visible ip
    m_have_address = true;
    m_ip_address = 0;

#ifdef ALLOW_DEBUG_COMMANDS
    m_last_stat_request_time = 0;
#endif

    //configure self
    // m_net_server.get_config_object().m_pcommands_handler = this;
    // m_net_server.get_config_object().m_invoke_timeout = CryptoNote::P2P_DEFAULT_INVOKE_TIMEOUT;

    //try to bind
    logger(INFO) << "Binding on " << m_bind_ip << ":" << m_port;
    m_listeningPort = Common::fromString<uint16_t>(m_port);

    m_listener = System::TcpListener(m_dispatcher, System::Ipv4Address(m_bind_ip), static_cast<uint16_t>(m_listeningPort));

    logger(INFO) << "Net service binded on " << m_bind_ip << ":" << m_listeningPort;

    if(m_external_port)
      logger(INFO) << "External port defined as " << m_external_port;

    addPortMapping(logger, m_listeningPort);

    return true;
  }
  //-----------------------------------------------------------------------------------

  CryptoNote::CryptoNoteProtocolHandler& NodeServer::get_payload_object()
  {
    return m_payload_handler;
  }
  //-----------------------------------------------------------------------------------

  bool NodeServer::run() {
    logger(INFO) << "Starting node_server";

    m_workingContextGroup.spawn(std::bind(&NodeServer::acceptLoop, this));
    m_workingContextGroup.spawn(std::bind(&NodeServer::onIdle, this));
    m_workingContextGroup.spawn(std::bind(&NodeServer::timedSyncLoop, this));
    m_workingContextGroup.spawn(std::bind(&NodeServer::timeoutLoop, this));

    m_stopEvent.wait();

    logger(INFO) << "Stopping NodeServer and its " << m_connections.size() << " connections...";
    safeInterrupt(m_workingContextGroup);
    m_workingContextGroup.wait();

    logger(INFO) << "NodeServer loop stopped";
    return true;
  }

  //-----------------------------------------------------------------------------------

  uint64_t NodeServer::get_connections_count() {
    return m_connections.size();
  }
  //-----------------------------------------------------------------------------------

  bool NodeServer::deinit()  {
    return store_config();
  }

  //-----------------------------------------------------------------------------------

  bool NodeServer::store_config()
  {
    try {
      if (!Tools::create_directories_if_necessary(m_config_folder)) {
        logger(INFO) << "Failed to create data directory: " << m_config_folder;
        return false;
      }

      std::string state_file_path = m_config_folder + "/" + m_p2p_state_filename;
      std::ofstream p2p_data;
      p2p_data.open(state_file_path, std::ios_base::binary | std::ios_base::out | std::ios::trunc);
      if (p2p_data.fail())  {
        logger(INFO) << "Failed to save config to file " << state_file_path;
        return false;
      };

      StdOutputStream stream(p2p_data);
      BinaryOutputStreamSerializer a(stream);
      CryptoNote::serialize(*this, a);
      return true;
    } catch (const std::exception& e) {
      logger(WARNING) << "store_config failed: " << e.what();
    }

    return false;
  }
  //-----------------------------------------------------------------------------------

  bool NodeServer::sendStopSignal()  {
    m_stop = true;

    m_dispatcher.remoteSpawn([this] {
      m_stopEvent.set();
      m_payload_handler.stop();
    });

    logger(INFO, BRIGHT_YELLOW) << "Stop signal sent, please only EXIT or CTRL+C one time to avoid stalling the shutdown process.";
    return true;
  }

  //-----------------------------------------------------------------------------------
  bool NodeServer::handshake(CryptoNote::LevinProtocol& proto, P2pConnectionContext& context, bool just_take_peerlist) {
    COMMAND_HANDSHAKE::request arg;
    COMMAND_HANDSHAKE::response rsp;
    get_local_node_data(arg.node_data);
    m_payload_handler.get_payload_sync_data(arg.payload_data);

    if (!proto.invoke(COMMAND_HANDSHAKE::ID, arg, rsp)) {
      logger(Logging::DEBUGGING) << context << "A daemon on the network has departed. MSG: Failed to invoke COMMAND_HANDSHAKE, closing connection.";
      return false;
    }

    context.version = rsp.node_data.version;

    if (rsp.node_data.network_id != m_network_id) {
      logger(Logging::DEBUGGING) << context << "COMMAND_HANDSHAKE Failed, wrong network! (" << rsp.node_data.network_id << "), closing connection.";
      return false;
    }

    if (rsp.node_data.version < CryptoNote::P2P_MINIMUM_VERSION) {
      logger(Logging::DEBUGGING) << context << "COMMAND_HANDSHAKE Failed, peer is wrong version! (" << std::to_string(rsp.node_data.version) << "), closing connection.";
      return false;
    } else if ((rsp.node_data.version - CryptoNote::P2P_CURRENT_VERSION) >= CryptoNote::P2P_UPGRADE_WINDOW) {
      logger(Logging::WARNING) << context << "COMMAND_HANDSHAKE Warning, your software may be out of date. Please visit: "
        << CryptoNote::LATEST_VERSION_URL << " for the latest version.";
    }

    if (!handle_remote_peerlist(rsp.local_peerlist, rsp.node_data.local_time, context)) {
      logger(Logging::ERROR) << context << "COMMAND_HANDSHAKE: failed to handle_remote_peerlist(...), closing connection.";
      return false;
    }

    if (just_take_peerlist) {
      return true;
    }

    if (!m_payload_handler.process_payload_sync_data(rsp.payload_data, context, true)) {
      logger(Logging::ERROR) << context << "COMMAND_HANDSHAKE invoked, but process_payload_sync_data returned false, dropping connection.";
      return false;
    }

    context.peerId = rsp.node_data.peer_id;
    m_peerlist.set_peer_just_seen(rsp.node_data.peer_id, context.m_remote_ip, context.m_remote_port);

    if (rsp.node_data.peer_id == m_config.m_peer_id)  {
      logger(Logging::TRACE) << context << "Connection to self detected, dropping connection";
      return false;
    }

    logger(Logging::DEBUGGING) << context << "COMMAND_HANDSHAKE INVOKED OK";
    return true;
  }


  bool NodeServer::timedSync() {
    COMMAND_TIMED_SYNC::request arg = boost::value_initialized<COMMAND_TIMED_SYNC::request>();
    m_payload_handler.get_payload_sync_data(arg.payload_data);
    auto cmdBuf = LevinProtocol::encode<COMMAND_TIMED_SYNC::request>(arg);

    forEachConnection([&](P2pConnectionContext& conn) {
      if (conn.peerId &&
          (conn.m_state == CryptoNoteConnectionContext::state_normal ||
           conn.m_state == CryptoNoteConnectionContext::state_idle)) {
        conn.pushMessage(P2pMessage(P2pMessage::COMMAND, COMMAND_TIMED_SYNC::ID, cmdBuf));
      }
    });

    return true;
  }

  bool NodeServer::handleTimedSyncResponse(const BinaryArray& in, P2pConnectionContext& context) {
    COMMAND_TIMED_SYNC::response rsp;
    if (!LevinProtocol::decode<COMMAND_TIMED_SYNC::response>(in, rsp)) {
      return false;
    }

    if (!handle_remote_peerlist(rsp.local_peerlist, rsp.local_time, context)) {
      logger(Logging::ERROR) << context << "COMMAND_TIMED_SYNC: failed to handle_remote_peerlist(...), closing connection.";
      return false;
    }

    if (!context.m_is_income) {
      m_peerlist.set_peer_just_seen(context.peerId, context.m_remote_ip, context.m_remote_port);
    }

    if (!m_payload_handler.process_payload_sync_data(rsp.payload_data, context, false)) {
      return false;
    }

    return true;
  }

  void NodeServer::forEachConnection(std::function<void(P2pConnectionContext&)> action) {

    // create copy of connection ids because the list can be changed during action
    std::vector<boost::uuids::uuid> connectionIds;
    connectionIds.reserve(m_connections.size());
    for (const auto& c : m_connections) {
      connectionIds.push_back(c.first);
    }

    for (const auto& connId : connectionIds) {
      auto it = m_connections.find(connId);
      if (it != m_connections.end()) {
        action(it->second);
      }
    }
  }

  //-----------------------------------------------------------------------------------
  bool NodeServer::is_peer_used(const PeerlistEntry& peer) {
    if(m_config.m_peer_id == peer.id)
      return true; //dont make connections to ourself

    for (const auto& kv : m_connections) {
      const auto& cntxt = kv.second;
      if(cntxt.peerId == peer.id || (!cntxt.m_is_income && peer.adr.ip == cntxt.m_remote_ip && peer.adr.port == cntxt.m_remote_port)) {
        return true;
      }
    }
    return false;
  }
  //-----------------------------------------------------------------------------------

  bool NodeServer::is_addr_connected(const NetworkAddress& peer) {
    for (const auto& conn : m_connections) {
      if (!conn.second.m_is_income && peer.ip == conn.second.m_remote_ip && peer.port == conn.second.m_remote_port) {
        return true;
      }
    }
    return false;
  }


  bool NodeServer::try_to_connect_and_handshake_with_new_peer(const NetworkAddress& na, bool just_take_peerlist, uint64_t last_seen_stamp, bool white)  {

    logger(DEBUGGING) << "Connecting to " << na << " (white=" << white << ", last_seen: "
        << (last_seen_stamp ? Common::timeIntervalToString(time(NULL) - last_seen_stamp) : "never") << ")...";

    try {
      System::TcpConnection connection;

      try {
        System::Context<System::TcpConnection> connectionContext(m_dispatcher, [&] {
          System::TcpConnector connector(m_dispatcher);
          return connector.connect(System::Ipv4Address(Common::ipAddressToString(na.ip)), static_cast<uint16_t>(na.port));
        });

        System::Context<> timeoutContext(m_dispatcher, [&] {
          System::Timer(m_dispatcher).sleep(std::chrono::milliseconds(m_config.m_net_config.connection_timeout));
          logger(DEBUGGING) << "Connection to " << na <<" timed out, interrupt it";
          safeInterrupt(connectionContext);
        });

        connection = std::move(connectionContext.get());
      } catch (System::InterruptedException&) {
        logger(DEBUGGING) << "Connection timed out";
        return false;
      }

      P2pConnectionContext ctx(m_dispatcher, logger.getLogger(), std::move(connection));

      ctx.m_connection_id = boost::uuids::random_generator()();
      ctx.m_remote_ip = na.ip;
      ctx.m_remote_port = na.port;
      ctx.m_is_income = false;
      ctx.m_started = time(nullptr);


      try {
        System::Context<bool> handshakeContext(m_dispatcher, [&] {
          CryptoNote::LevinProtocol proto(ctx.connection);
          return handshake(proto, ctx, just_take_peerlist);
        });

        System::Context<> timeoutContext(m_dispatcher, [&] {
          // Here we use connection_timeout * 3, one for this handshake, and two for back ping from peer.
          System::Timer(m_dispatcher).sleep(std::chrono::milliseconds(m_config.m_net_config.connection_timeout * 3));
          logger(DEBUGGING) << "Handshake with " << na << " timed out, interrupt it";
          safeInterrupt(handshakeContext);
        });

        if (!handshakeContext.get()) {
          logger(DEBUGGING) << "Failed to HANDSHAKE with peer " << na;
          return false;
        }
      } catch (System::InterruptedException&) {
        logger(DEBUGGING) << "Handshake timed out";
        return false;
      }

      if (just_take_peerlist) {
        logger(Logging::DEBUGGING, Logging::BRIGHT_GREEN) << ctx << "CONNECTION HANDSHAKED OK AND CLOSED.";
        return true;
      }

      PeerlistEntry pe_local = boost::value_initialized<PeerlistEntry>();
      pe_local.adr = na;
      pe_local.id = ctx.peerId;
      pe_local.last_seen = time(nullptr);
      m_peerlist.append_with_peer_white(pe_local);

      if (m_stop) {
        throw System::InterruptedException();
      }

      auto iter = m_connections.emplace(ctx.m_connection_id, std::move(ctx)).first;
      const boost::uuids::uuid& connectionId = iter->first;
      P2pConnectionContext& connectionContext = iter->second;

      m_workingContextGroup.spawn(std::bind(&NodeServer::connectionHandler, this, std::cref(connectionId), std::ref(connectionContext)));

      return true;
    } catch (System::InterruptedException&) {
      logger(DEBUGGING) << "Connection process interrupted";
      throw;
    } catch (const std::exception& e) {
      logger(DEBUGGING) << "Connection to " << na << " failed: " << e.what();
    }

    return false;
  }

  //-----------------------------------------------------------------------------------
  bool NodeServer::make_new_connection_from_peerlist(bool use_white_list)
  {
    size_t local_peers_count = use_white_list ? m_peerlist.get_white_peers_count():m_peerlist.get_gray_peers_count();
    if(!local_peers_count)
      return false;//no peers

    size_t max_random_index = std::min<uint64_t>(local_peers_count -1, 20);

    std::set<size_t> tried_peers;

    size_t try_count = 0;
    size_t rand_count = 0;
    while(rand_count < (max_random_index+1)*3 &&  try_count < 10 && !m_stop) {
      ++rand_count;
      size_t random_index = get_random_index_with_fixed_probability(max_random_index);
      if (!(random_index < local_peers_count)) { logger(ERROR, BRIGHT_RED) << "random_starter_index < peers_local.size() failed!!"; return false; }

      if(tried_peers.count(random_index))
        continue;

      tried_peers.insert(random_index);
      PeerlistEntry pe = boost::value_initialized<PeerlistEntry>();
      bool r = use_white_list ? m_peerlist.get_white_peer_by_index(pe, random_index):m_peerlist.get_gray_peer_by_index(pe, random_index);
      if (!(r)) { logger(ERROR, BRIGHT_RED) << "Failed to get random peer from peerlist(white:" << use_white_list << ")"; return false; }

      ++try_count;

      if(is_peer_used(pe))
        continue;

      logger(DEBUGGING) << "Selected peer: " << pe.id << " " << pe.adr << " [white=" << use_white_list
                    << "] last_seen: " << (pe.last_seen ? Common::timeIntervalToString(time(NULL) - pe.last_seen) : "never");

      if(!try_to_connect_and_handshake_with_new_peer(pe.adr, false, pe.last_seen, use_white_list))
        continue;

      return true;
    }
    return false;
  }
  //-----------------------------------------------------------------------------------

  bool NodeServer::connections_maker()
  {
    if (!connect_to_peerlist(m_exclusive_peers)) {
      return false;
    }

    if (!m_exclusive_peers.empty()) {
      return true;
    }

    if(!m_peerlist.get_white_peers_count() && m_seed_nodes.size()) {
      size_t try_count = 0;
      size_t current_index = Random::randomValue<size_t>() % m_seed_nodes.size();

      while(true) {
        if(try_to_connect_and_handshake_with_new_peer(m_seed_nodes[current_index], true))
          break;

        if(++try_count > m_seed_nodes.size()) {
          logger(ERROR) << "Failed to connect to any of seed peers, continuing without seeds";
          break;
        }
        if(++current_index >= m_seed_nodes.size())
          current_index = 0;
      }
    }

    if (!connect_to_peerlist(m_priority_peers)) return false;

    size_t expected_white_connections = (m_config.m_net_config.connections_count * CryptoNote::P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT) / 100;

    size_t conn_count = get_outgoing_connections_count();
    if(conn_count < m_config.m_net_config.connections_count)
    {
      if(conn_count < expected_white_connections)
      {
        //start from white list
        if(!make_expected_connections_count(true, expected_white_connections))
          return false;
        //and then do grey list
        if(!make_expected_connections_count(false, m_config.m_net_config.connections_count))
          return false;
      }else
      {
        //start from grey list
        if(!make_expected_connections_count(false, m_config.m_net_config.connections_count))
          return false;
        //and then do white list
        if(!make_expected_connections_count(true, m_config.m_net_config.connections_count))
          return false;
      }
    }

    return true;
  }
  //-----------------------------------------------------------------------------------

  bool NodeServer::make_expected_connections_count(bool white_list, size_t expected_connections)
  {
    size_t conn_count = get_outgoing_connections_count();
    //add new connections from white peers
    while(conn_count < expected_connections)
    {
      if(m_stopEvent.get())
        return false;

      if(!make_new_connection_from_peerlist(white_list))
        break;
      conn_count = get_outgoing_connections_count();
    }
    return true;
  }

  //-----------------------------------------------------------------------------------
  size_t NodeServer::get_outgoing_connections_count() {
    size_t count = 0;
    for (const auto& cntxt : m_connections) {
      if (!cntxt.second.m_is_income)
        ++count;
    }
    return count;
  }

  //-----------------------------------------------------------------------------------
  bool NodeServer::idle_worker() {
    try {
      m_connections_maker_interval.call(std::bind(&NodeServer::connections_maker, this));
      m_peerlist_store_interval.call(std::bind(&NodeServer::store_config, this));
    } catch (std::exception& e) {
      logger(DEBUGGING) << "exception in idle_worker: " << e.what();
    }
    return true;
  }

  //-----------------------------------------------------------------------------------
  bool NodeServer::fix_time_delta(std::list<PeerlistEntry>& local_peerlist, time_t local_time, int64_t& delta)
  {
    //fix time delta
    time_t now = 0;
    time(&now);
    delta = now - local_time;

    BOOST_FOREACH(PeerlistEntry& be, local_peerlist)
    {
      if(be.last_seen > uint64_t(local_time))
      {
        logger(DEBUGGING) << "FOUND FUTURE peerlist for entry " << be.adr << " last_seen: " << be.last_seen << ", local_time(on remote node):" << local_time;
        return false;
      }
      be.last_seen += delta;
    }
    return true;
  }

  //-----------------------------------------------------------------------------------

  bool NodeServer::handle_remote_peerlist(const std::list<PeerlistEntry>& peerlist, time_t local_time, const CryptoNoteConnectionContext& context)
  {
    int64_t delta = 0;
    std::list<PeerlistEntry> peerlist_ = peerlist;
    if(!fix_time_delta(peerlist_, local_time, delta))
      return false;
    logger(Logging::TRACE) << context << "REMOTE PEERLIST: TIME_DELTA: " << delta << ", remote peerlist size=" << peerlist_.size();
    logger(Logging::TRACE) << context << "REMOTE PEERLIST: " <<  print_peerlist_to_string(peerlist_);
    return m_peerlist.merge_peerlist(peerlist_);
  }
  //-----------------------------------------------------------------------------------

  bool NodeServer::get_local_node_data(basic_node_data& node_data)
  {
    node_data.version = CryptoNote::P2P_CURRENT_VERSION;
    time_t local_time;
    time(&local_time);
    node_data.local_time = local_time;
    node_data.peer_id = m_config.m_peer_id;
    if(!m_hide_my_port)
      node_data.my_port = m_external_port ? m_external_port : m_listeningPort;
    else
      node_data.my_port = 0;
    node_data.network_id = m_network_id;
    return true;
  }
  //-----------------------------------------------------------------------------------
#ifdef ALLOW_DEBUG_COMMANDS

  bool NodeServer::check_trust(const proof_of_trust &tr) {
    uint64_t local_time = time(NULL);
    uint64_t time_delata = local_time > tr.time ? local_time - tr.time : tr.time - local_time;

    if (time_delata > 24 * 60 * 60) {
      logger(ERROR) << "check_trust failed to check time conditions, local_time=" << local_time << ", proof_time=" << tr.time;
      return false;
    }

    if (m_last_stat_request_time >= tr.time) {
      logger(ERROR) << "check_trust failed to check time conditions, last_stat_request_time=" << m_last_stat_request_time << ", proof_time=" << tr.time;
      return false;
    }

    if (m_config.m_peer_id != tr.peer_id) {
      logger(ERROR) << "check_trust failed: peer_id mismatch (passed " << tr.peer_id << ", expected " << m_config.m_peer_id << ")";
      return false;
    }

    Crypto::PublicKey pk;
    Common::podFromHex(CryptoNote::P2P_STAT_TRUSTED_PUB_KEY, pk);
    Crypto::Hash h = get_proof_of_trust_hash(tr);
    if (!Crypto::check_signature(h, pk, tr.sign)) {
      logger(ERROR) << "check_trust failed: sign check failed";
      return false;
    }

    //update last request time
    m_last_stat_request_time = tr.time;
    return true;
  }
  //-----------------------------------------------------------------------------------

  int NodeServer::handle_get_stat_info(int command, COMMAND_REQUEST_STAT_INFO::request& arg, COMMAND_REQUEST_STAT_INFO::response& rsp, P2pConnectionContext& context)
  {
    if(!check_trust(arg.tr)) {
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    }
    rsp.connections_count = get_connections_count();
    rsp.incoming_connections_count = rsp.connections_count - get_outgoing_connections_count();
    rsp.version = PROJECT_VERSION_LONG;
    rsp.os_version = Tools::get_os_version_string();
    rsp.payload_info = m_payload_handler.getStatistics();
    return 1;
  }
  //-----------------------------------------------------------------------------------

  int NodeServer::handle_get_network_state(int command, COMMAND_REQUEST_NETWORK_STATE::request& arg, COMMAND_REQUEST_NETWORK_STATE::response& rsp, P2pConnectionContext& context)
  {
    if(!check_trust(arg.tr)) {
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    }

    for (const auto& cntxt : m_connections) {
      connection_entry ce;
      ce.adr.ip = cntxt.second.m_remote_ip;
      ce.adr.port = cntxt.second.m_remote_port;
      ce.id = cntxt.second.peerId;
      ce.is_income = cntxt.second.m_is_income;
      rsp.connections_list.push_back(ce);
    }

    m_peerlist.get_peerlist_full(rsp.local_peerlist_gray, rsp.local_peerlist_white);
    rsp.my_id = m_config.m_peer_id;
    rsp.local_time = time(NULL);
    return 1;
  }
  //-----------------------------------------------------------------------------------

  int NodeServer::handle_get_peer_id(int command, COMMAND_REQUEST_PEER_ID::request& arg, COMMAND_REQUEST_PEER_ID::response& rsp, P2pConnectionContext& context)
  {
    rsp.my_id = m_config.m_peer_id;
    return 1;
  }
#endif

  //-----------------------------------------------------------------------------------

  void NodeServer::relay_notify_to_all(int command, const BinaryArray& data_buff, const boost::uuids::uuid* excludeConnection) {
    boost::uuids::uuid excludeId = excludeConnection ? *excludeConnection : boost::value_initialized<boost::uuids::uuid>();

    forEachConnection([&](P2pConnectionContext& conn) {
      if (conn.peerId && conn.m_connection_id != excludeId &&
          (conn.m_state == CryptoNoteConnectionContext::state_normal ||
           conn.m_state == CryptoNoteConnectionContext::state_synchronizing)) {
        conn.pushMessage(P2pMessage(P2pMessage::NOTIFY, command, data_buff));
      }
    });
  }

  //-----------------------------------------------------------------------------------
  bool NodeServer::invoke_notify_to_peer(int command, const BinaryArray& buffer, const CryptoNoteConnectionContext& context) {
    auto it = m_connections.find(context.m_connection_id);
    if (it == m_connections.end()) {
      return false;
    }

    it->second.pushMessage(P2pMessage(P2pMessage::NOTIFY, command, buffer));

    return true;
  }

  //-----------------------------------------------------------------------------------
  bool NodeServer::try_ping(basic_node_data& node_data, P2pConnectionContext& context) {
    if(!node_data.my_port) {
      return false;
    }

    uint32_t actual_ip =  context.m_remote_ip;
    if(!m_peerlist.is_ip_allowed(actual_ip)) {
      return false;
    }

    auto ip = Common::ipAddressToString(actual_ip);
    auto port = node_data.my_port;
    auto peerId = node_data.peer_id;

    try {
      COMMAND_PING::request req;
      COMMAND_PING::response rsp;
      System::Context<> pingContext(m_dispatcher, [&] {
        System::TcpConnector connector(m_dispatcher);
        auto connection = connector.connect(System::Ipv4Address(ip), static_cast<uint16_t>(port));
        LevinProtocol(connection).invoke(COMMAND_PING::ID, req, rsp);
      });

      System::Context<> timeoutContext(m_dispatcher, [&] {
        System::Timer(m_dispatcher).sleep(std::chrono::milliseconds(m_config.m_net_config.connection_timeout * 2));
        logger(DEBUGGING) << context << "Back ping timed out" << ip << ":" << port;
        safeInterrupt(pingContext);
      });

      pingContext.get();

      if (rsp.status != PING_OK_RESPONSE_STATUS_TEXT || peerId != rsp.peer_id) {
        logger(DEBUGGING) << context << "Back ping invoke wrong response \"" << rsp.status << "\" from" << ip
                                   << ":" << port << ", hsh_peer_id=" << peerId << ", rsp.peer_id=" << rsp.peer_id;
        return false;
      }
    } catch (std::exception& e) {
      logger(DEBUGGING) << context << "Back ping connection to " << ip << ":" << port << " failed: " << e.what();
      return false;
    }

    return true;
  }

  //-----------------------------------------------------------------------------------
  int NodeServer::handle_timed_sync(int command, COMMAND_TIMED_SYNC::request& arg, COMMAND_TIMED_SYNC::response& rsp, P2pConnectionContext& context)
  {
    if(!m_payload_handler.process_payload_sync_data(arg.payload_data, context, false)) {
      logger(Logging::ERROR) << context << "Failed to process_payload_sync_data(), dropping connection";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    }

    //fill response
    rsp.local_time = time(NULL);
    m_peerlist.get_peerlist_head(rsp.local_peerlist);
    m_payload_handler.get_payload_sync_data(rsp.payload_data);
    logger(Logging::TRACE) << context << "COMMAND_TIMED_SYNC";
    return 1;
  }
  //-----------------------------------------------------------------------------------

  int NodeServer::handle_handshake(int command, COMMAND_HANDSHAKE::request& arg, COMMAND_HANDSHAKE::response& rsp, P2pConnectionContext& context)
  {
    context.version = arg.node_data.version;

    if (arg.node_data.network_id != m_network_id) {
      logger(Logging::DEBUGGING) << context << "WRONG NETWORK AGENT CONNECTED! id=" << arg.node_data.network_id;
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    }

    if (arg.node_data.version < CryptoNote::P2P_MINIMUM_VERSION) {
      logger(Logging::DEBUGGING) << context << "UNSUPPORTED NETWORK AGENT VERSION CONNECTED! version=" << std::to_string(arg.node_data.version);
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    } else if (arg.node_data.version > CryptoNote::P2P_CURRENT_VERSION) {
      logger(Logging::WARNING) << context << "Our software may be out of date. Please visit: "
        << CryptoNote::LATEST_VERSION_URL << " for the latest version.";
    }

    if(!context.m_is_income) {
      logger(Logging::ERROR) << context << "COMMAND_HANDSHAKE came not from incoming connection";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    }

    if(context.peerId) {
      logger(Logging::ERROR) << context << "COMMAND_HANDSHAKE came, but seems that connection already have associated peer_id (double COMMAND_HANDSHAKE?)";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    }

    if(!m_payload_handler.process_payload_sync_data(arg.payload_data, context, true))  {
      logger(Logging::ERROR) << context << "COMMAND_HANDSHAKE came, but process_payload_sync_data returned false, dropping connection.";
      context.m_state = CryptoNoteConnectionContext::state_shutdown;
      return 1;
    }
    //associate peer_id with this connection
    context.peerId = arg.node_data.peer_id;

    if(arg.node_data.peer_id != m_config.m_peer_id && arg.node_data.my_port) {
      uint64_t peer_id_l = arg.node_data.peer_id;
      uint32_t port_l = arg.node_data.my_port;

      if (try_ping(arg.node_data, context)) {
          //called only(!) if success pinged, update local peerlist
          PeerlistEntry pe;
          pe.adr.ip = context.m_remote_ip;
          pe.adr.port = port_l;
          pe.last_seen = time(nullptr);
          pe.id = peer_id_l;
          m_peerlist.append_with_peer_white(pe);

          logger(Logging::TRACE) << context << "BACK PING SUCCESS, " << Common::ipAddressToString(context.m_remote_ip) << ":" << port_l << " added to whitelist";
      }
    }

    //fill response
    m_peerlist.get_peerlist_head(rsp.local_peerlist);
    get_local_node_data(rsp.node_data);
    m_payload_handler.get_payload_sync_data(rsp.payload_data);

    logger(Logging::DEBUGGING, Logging::BRIGHT_GREEN) << "COMMAND_HANDSHAKE";
    return 1;
  }
  //-----------------------------------------------------------------------------------

  int NodeServer::handle_ping(int command, COMMAND_PING::request& arg, COMMAND_PING::response& rsp, P2pConnectionContext& context)
  {
    logger(Logging::TRACE) << context << "COMMAND_PING";
    rsp.status = PING_OK_RESPONSE_STATUS_TEXT;
    rsp.peer_id = m_config.m_peer_id;
    return 1;
  }
  //-----------------------------------------------------------------------------------

  bool NodeServer::log_peerlist()
  {
    std::list<PeerlistEntry> pl_wite;
    std::list<PeerlistEntry> pl_gray;
    m_peerlist.get_peerlist_full(pl_gray, pl_wite);
    logger(INFO) << ENDL << "Peerlist white:" << ENDL << print_peerlist_to_string(pl_wite) << ENDL << "Peerlist gray:" << ENDL << print_peerlist_to_string(pl_gray) ;
    return true;
  }
  //-----------------------------------------------------------------------------------

  bool NodeServer::log_connections() {
    logger(INFO) << "Connections: \r\n" << print_connections_container() ;
    return true;
  }
  //-----------------------------------------------------------------------------------

  std::string NodeServer::print_connections_container() {

    std::stringstream ss;

    for (const auto& cntxt : m_connections) {
      ss << Common::ipAddressToString(cntxt.second.m_remote_ip) << ":" << cntxt.second.m_remote_port
        << " \t\tpeer_id " << cntxt.second.peerId
        << " \t\tconn_id " << cntxt.second.m_connection_id << (cntxt.second.m_is_income ? " INCOMING" : " OUTGOING")
        << std::endl;
    }

    return ss.str();
  }
  //-----------------------------------------------------------------------------------

  void NodeServer::on_connection_new(P2pConnectionContext& context)
  {
    logger(TRACE) << context << "NEW CONNECTION";
    m_payload_handler.onConnectionOpened(context);
  }
  //-----------------------------------------------------------------------------------

  void NodeServer::on_connection_close(P2pConnectionContext& context)
  {
    logger(TRACE) << context << "CLOSE CONNECTION";
    m_payload_handler.onConnectionClosed(context);
  }

  bool NodeServer::connect_to_peerlist(const std::vector<NetworkAddress>& peers)
  {
    for(const auto& na: peers) {
      if (!is_addr_connected(na)) {
        try_to_connect_and_handshake_with_new_peer(na);
      }
    }

    return true;
  }

  void NodeServer::acceptLoop() {
    while (!m_stop) {
      try {
        P2pConnectionContext ctx(m_dispatcher, logger.getLogger(), m_listener.accept());
        ctx.m_connection_id = boost::uuids::random_generator()();
        ctx.m_is_income = true;
        ctx.m_started = time(nullptr);

        auto addressAndPort = ctx.connection.getPeerAddressAndPort();
        ctx.m_remote_ip = hostToNetwork(addressAndPort.first.getValue());
        ctx.m_remote_port = addressAndPort.second;

        auto iter = m_connections.emplace(ctx.m_connection_id, std::move(ctx)).first;
        const boost::uuids::uuid& connectionId = iter->first;
        P2pConnectionContext& connection = iter->second;

        m_workingContextGroup.spawn(std::bind(&NodeServer::connectionHandler, this, std::cref(connectionId), std::ref(connection)));
      } catch (System::InterruptedException&) {
        logger(DEBUGGING) << "acceptLoop() is interrupted";
        break;
      } catch (const std::exception& e) {
        logger(DEBUGGING) << "Exception in acceptLoop: " << e.what();
      }
    }

    logger(DEBUGGING) << "acceptLoop finished";
  }

  void NodeServer::onIdle() {
    logger(DEBUGGING) << "onIdle started";

    while (!m_stop) {
      try {
        idle_worker();
        m_idleTimer.sleep(std::chrono::seconds(1));
      } catch (System::InterruptedException&) {
        logger(DEBUGGING) << "onIdle() is interrupted";
        break;
      } catch (std::exception& e) {
        logger(WARNING) << "Exception in onIdle: " << e.what();
      }
    }

    logger(DEBUGGING) << "onIdle finished";
  }

  void NodeServer::timeoutLoop() {
    try {
      while (!m_stop) {
        m_timeoutTimer.sleep(std::chrono::seconds(10));
        auto now = P2pConnectionContext::Clock::now();

        for (auto& kv : m_connections) {
          auto& ctx = kv.second;
          if (ctx.writeDuration(now) > P2P_DEFAULT_INVOKE_TIMEOUT) {
            logger(DEBUGGING) << ctx << "write operation timed out, stopping connection";
            safeInterrupt(ctx);
          }
        }
      }
    } catch (System::InterruptedException&) {
      logger(DEBUGGING) << "timeoutLoop() is interrupted";
    } catch (std::exception& e) {
      logger(WARNING) << "Exception in timeoutLoop: " << e.what();
    } catch (...) {
      logger(WARNING) << "Unknown exception in timeoutLoop";
    }
  }

  void NodeServer::timedSyncLoop() {
    try {
      for (;;) {
        m_timedSyncTimer.sleep(std::chrono::seconds(P2P_DEFAULT_HANDSHAKE_INTERVAL));
        timedSync();
      }
    } catch (System::InterruptedException&) {
      logger(DEBUGGING) << "timedSyncLoop() is interrupted";
    } catch (std::exception& e) {
      logger(WARNING) << "Exception in timedSyncLoop: " << e.what();
    }

    logger(DEBUGGING) << "timedSyncLoop finished";
  }

  void NodeServer::connectionHandler(const boost::uuids::uuid& connectionId, P2pConnectionContext& ctx) {
    // This inner context is necessary in order to stop connection handler at any moment
    System::Context<> context(m_dispatcher, [this, &connectionId, &ctx] {
      System::Context<> writeContext(m_dispatcher, std::bind(&NodeServer::writeHandler, this, std::ref(ctx)));

      try {
        on_connection_new(ctx);

        LevinProtocol proto(ctx.connection);
        LevinProtocol::Command cmd;

        for (;;) {
          if (ctx.m_state == CryptoNoteConnectionContext::state_sync_required) {
            ctx.m_state = CryptoNoteConnectionContext::state_synchronizing;
            m_payload_handler.start_sync(ctx);
          } else if (ctx.m_state == CryptoNoteConnectionContext::state_pool_sync_required) {
            ctx.m_state = CryptoNoteConnectionContext::state_normal;
            m_payload_handler.requestMissingPoolTransactions(ctx);
          }

          if (!proto.readCommand(cmd)) {
            break;
          }

          BinaryArray response;
          bool handled = false;
          auto retcode = handleCommand(cmd, response, ctx, handled);

          // send response
          if (cmd.needReply()) {
            if (!handled) {
              retcode = static_cast<int32_t>(LevinError::ERROR_CONNECTION_HANDLER_NOT_DEFINED);
              response.clear();
            }

            ctx.pushMessage(P2pMessage(P2pMessage::REPLY, cmd.command, std::move(response), retcode));
          }

          if (ctx.m_state == CryptoNoteConnectionContext::state_shutdown) {
            break;
          }
        }
      } catch (System::InterruptedException&) {
        logger(DEBUGGING) << ctx << "connectionHandler() inner context is interrupted";
      } catch (std::exception& e) {
        logger(DEBUGGING) << ctx << "Exception in connectionHandler: " << e.what();
      }

      safeInterrupt(ctx);
      safeInterrupt(writeContext);
      writeContext.wait();

      on_connection_close(ctx);
      m_connections.erase(connectionId);
    });

    ctx.context = &context;

    try {
      context.get();
    } catch (System::InterruptedException&) {
      logger(DEBUGGING) << "connectionHandler() is interrupted";
    } catch (std::exception& e) {
      logger(WARNING) << "connectionHandler() throws exception: " << e.what();
    } catch (...) {
      logger(WARNING) << "connectionHandler() throws unknown exception";
    }
  }

  void NodeServer::writeHandler(P2pConnectionContext& ctx) {
    logger(DEBUGGING) << ctx << "writeHandler started";

    try {
      LevinProtocol proto(ctx.connection);

      for (;;) {
        auto msgs = ctx.popBuffer();
        if (msgs.empty()) {
          break;
        }

        for (const auto& msg : msgs) {
          logger(DEBUGGING) << ctx << "msg " << msg.type << ':' << msg.command;
          switch (msg.type) {
          case P2pMessage::COMMAND:
            proto.sendMessage(msg.command, msg.buffer, true);
            break;
          case P2pMessage::NOTIFY:
            proto.sendMessage(msg.command, msg.buffer, false);
            break;
          case P2pMessage::REPLY:
            proto.sendReply(msg.command, msg.buffer, msg.returnCode);
            break;
          default:
            assert(false);
          }
        }
      }
    } catch (System::InterruptedException&) {
      // connection stopped
      logger(DEBUGGING) << ctx << "writeHandler() is interrupted";
    } catch (std::exception& e) {
      logger(DEBUGGING) << ctx << "error during write: " << e.what();
      safeInterrupt(ctx); // stop connection on write error
    }

    logger(DEBUGGING) << ctx << "writeHandler finished";
  }

  template<typename T>
  void NodeServer::safeInterrupt(T& obj) {
    try {
      obj.interrupt();
    } catch (std::exception& e) {
      logger(WARNING) << "interrupt() throws exception: " << e.what();
    } catch (...) {
      logger(WARNING) << "interrupt() throws unknown exception";
    }
  }

}
