// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "NetNodeConfig.h"

#include <boost/utility/value_init.hpp>

#include <Common/Util.h>
#include "Common/CommandLine.h"
#include "Common/StringTools.h"
#include "crypto/crypto.h"
#include "CryptoNoteConfig.h"

namespace CryptoNote
{
    namespace
    {

        const command_line::arg_descriptor<std::string> arg_p2p_bind_ip = {"p2p-bind-ip",
                                                                           "Interface for p2p network protocol",
                                                                           "0.0.0.0"};
        const command_line::arg_descriptor<uint16_t> arg_p2p_bind_port = {"p2p-bind-port",
                                                                          "Port for p2p network protocol",
                                                                          P2P_DEFAULT_PORT};
        const command_line::arg_descriptor<uint16_t> arg_p2p_external_port = {"p2p-external-port",
                                                                              "External port for p2p network protocol (if port forwarding used with NAT)",
                                                                              0};
        const command_line::arg_descriptor<bool> arg_p2p_allow_local_ip = {"allow-local-ip",
                                                                           "Allow local ip add to peer list, mostly in debug purposes"};
        const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_add_peer = {"add-peer",
                                                                                          "Manually add peer to local peerlist"};
        const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_add_priority_node = {"add-priority-node",
                                                                                                   "Specify list of peers to connect to and attempt to keep the connection open"};
        const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_add_exclusive_node = {
                "add-exclusive-node", "Specify list of peers to connect to only."
                                      " If this option is given the options add-priority-node and seed-node are ignored"};
        const command_line::arg_descriptor<std::vector<std::string> > arg_p2p_seed_node = {"seed-node",
                                                                                           "Connect to a node to retrieve peer addresses, and disconnect"};
        const command_line::arg_descriptor<bool> arg_p2p_hide_my_port = {"hide-my-port",
                                                                         "Do not announce yourself as peerlist candidate",
                                                                         false, true};

        bool parsePeerFromString(NetworkAddress &pe, const std::string &node_addr)
        {
            return Common::parseIpAddressAndPort(pe.ip, pe.port, node_addr);
        }

        bool parsePeersAndAddToContainer(const boost::program_options::variables_map &vm,
                                         const command_line::arg_descriptor<std::vector<std::string>> &arg,
                                         std::vector<NetworkAddress> &container)
        {
            std::vector<std::string> peers = command_line::get_arg(vm, arg);

            for (const std::string &str: peers)
            {
                NetworkAddress na = boost::value_initialized<NetworkAddress>();
                if (!parsePeerFromString(na, str))
                {
                    return false;
                }
                container.push_back(na);
            }

            return true;
        }

    } //namespace

    void NetNodeConfig::initOptions(boost::program_options::options_description &desc)
    {
        command_line::add_arg(desc, arg_p2p_bind_ip);
        command_line::add_arg(desc, arg_p2p_bind_port);
        command_line::add_arg(desc, arg_p2p_external_port);
        command_line::add_arg(desc, arg_p2p_allow_local_ip);
        command_line::add_arg(desc, arg_p2p_add_peer);
        command_line::add_arg(desc, arg_p2p_add_priority_node);
        command_line::add_arg(desc, arg_p2p_add_exclusive_node);
        command_line::add_arg(desc, arg_p2p_seed_node);
        command_line::add_arg(desc, arg_p2p_hide_my_port);
    }

    NetNodeConfig::NetNodeConfig()
    {
        bindIp = "";
        bindPort = 0;
        externalPort = 0;
        allowLocalIp = false;
        hideMyPort = false;
        configFolder = Tools::getDefaultDataDirectory();
        testnet = false;
    }

    bool NetNodeConfig::init(const boost::program_options::variables_map &vm)
    {
        if (vm.count(arg_p2p_bind_ip.name) != 0 && (!vm[arg_p2p_bind_ip.name].defaulted() || bindIp.empty()))
        {
            bindIp = command_line::get_arg(vm, arg_p2p_bind_ip);
        }

        if (vm.count(arg_p2p_bind_port.name) != 0 && (!vm[arg_p2p_bind_port.name].defaulted() || bindPort == 0))
        {
            bindPort = command_line::get_arg(vm, arg_p2p_bind_port);
        }

        if (vm.count(arg_p2p_external_port.name) != 0 &&
            (!vm[arg_p2p_external_port.name].defaulted() || externalPort == 0))
        {
            externalPort = command_line::get_arg(vm, arg_p2p_external_port);
        }

        if (vm.count(arg_p2p_allow_local_ip.name) != 0 &&
            (!vm[arg_p2p_allow_local_ip.name].defaulted() || !allowLocalIp))
        {
            allowLocalIp = command_line::get_arg(vm, arg_p2p_allow_local_ip);
        }

        if (vm.count(command_line::arg_data_dir.name) != 0 &&
            (!vm[command_line::arg_data_dir.name].defaulted() || configFolder == Tools::getDefaultDataDirectory()))
        {
            configFolder = command_line::get_arg(vm, command_line::arg_data_dir);
        }

        p2pStateFilename = CryptoNote::parameters::P2P_NET_DATA_FILENAME;

        if (command_line::has_arg(vm, arg_p2p_add_peer))
        {
            std::vector<std::string> perrs = command_line::get_arg(vm, arg_p2p_add_peer);
            for (const std::string &pr_str: perrs)
            {
                PeerlistEntry pe = boost::value_initialized<PeerlistEntry>();
                pe.id = Crypto::rand<uint64_t>();
                if (!parsePeerFromString(pe.adr, pr_str))
                {
                    return false;
                }

                peers.push_back(pe);
            }
        }

        if (command_line::has_arg(vm, arg_p2p_add_exclusive_node))
        {
            if (!parsePeersAndAddToContainer(vm, arg_p2p_add_exclusive_node, exclusiveNodes))
                return false;
        }

        if (command_line::has_arg(vm, arg_p2p_add_priority_node))
        {
            if (!parsePeersAndAddToContainer(vm, arg_p2p_add_priority_node, priorityNodes))
                return false;
        }

        if (command_line::has_arg(vm, arg_p2p_seed_node))
        {
            if (!parsePeersAndAddToContainer(vm, arg_p2p_seed_node, seedNodes))
                return false;
        }

        if (command_line::has_arg(vm, arg_p2p_hide_my_port))
        {
            hideMyPort = true;
        }

        return true;
    }

    void NetNodeConfig::setTestnet(bool isTestnet)
    {
        testnet = isTestnet;
    }

    std::string NetNodeConfig::getP2pStateFilename() const
    {
        if (testnet)
        {
            return "testnet_" + p2pStateFilename;
        }

        return p2pStateFilename;
    }

    bool NetNodeConfig::getTestnet() const
    {
        return testnet;
    }

    std::string NetNodeConfig::getBindIp() const
    {
        return bindIp;
    }

    uint16_t NetNodeConfig::getBindPort() const
    {
        return bindPort;
    }

    uint16_t NetNodeConfig::getExternalPort() const
    {
        return externalPort;
    }

    bool NetNodeConfig::getAllowLocalIp() const
    {
        return allowLocalIp;
    }

    std::vector<PeerlistEntry> NetNodeConfig::getPeers() const
    {
        return peers;
    }

    std::vector<NetworkAddress> NetNodeConfig::getPriorityNodes() const
    {
        return priorityNodes;
    }

    std::vector<NetworkAddress> NetNodeConfig::getExclusiveNodes() const
    {
        return exclusiveNodes;
    }

    std::vector<NetworkAddress> NetNodeConfig::getSeedNodes() const
    {
        return seedNodes;
    }

    bool NetNodeConfig::getHideMyPort() const
    {
        return hideMyPort;
    }

    std::string NetNodeConfig::getConfigFolder() const
    {
        return configFolder;
    }

    void NetNodeConfig::setP2pStateFilename(const std::string &filename)
    {
        p2pStateFilename = filename;
    }

    void NetNodeConfig::setBindIp(const std::string &ip)
    {
        bindIp = ip;
    }

    void NetNodeConfig::setBindPort(uint16_t port)
    {
        bindPort = port;
    }

    void NetNodeConfig::setExternalPort(uint16_t port)
    {
        externalPort = port;
    }

    void NetNodeConfig::setAllowLocalIp(bool allow)
    {
        allowLocalIp = allow;
    }

    void NetNodeConfig::setPeers(const std::vector<PeerlistEntry> &peerList)
    {
        peers = peerList;
    }

    void NetNodeConfig::setPriorityNodes(const std::vector<NetworkAddress> &addresses)
    {
        priorityNodes = addresses;
    }

    void NetNodeConfig::setExclusiveNodes(const std::vector<NetworkAddress> &addresses)
    {
        exclusiveNodes = addresses;
    }

    void NetNodeConfig::setSeedNodes(const std::vector<NetworkAddress> &addresses)
    {
        seedNodes = addresses;
    }

    void NetNodeConfig::setHideMyPort(bool hide)
    {
        hideMyPort = hide;
    }

    void NetNodeConfig::setConfigFolder(const std::string &folder)
    {
        configFolder = folder;
    }


} //namespace nodetool
