// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include <boost/program_options.hpp>
#include "P2pProtocolTypes.h"

namespace CryptoNote
{

    class NetNodeConfig
    {
    public:
        NetNodeConfig();

        static void initOptions(boost::program_options::options_description &desc);

        bool init(const boost::program_options::variables_map &vm);

        std::string getP2pStateFilename() const;

        bool getTestnet() const;

        std::string getBindIp() const;

        uint16_t getBindPort() const;

        uint16_t getExternalPort() const;

        bool getAllowLocalIp() const;

        std::vector<PeerlistEntry> getPeers() const;

        std::vector<NetworkAddress> getPriorityNodes() const;

        std::vector<NetworkAddress> getExclusiveNodes() const;

        std::vector<NetworkAddress> getSeedNodes() const;

        bool getHideMyPort() const;

        std::string getConfigFolder() const;

        void setP2pStateFilename(const std::string &filename);

        void setTestnet(bool isTestnet);

        void setBindIp(const std::string &ip);

        void setBindPort(uint16_t port);

        void setExternalPort(uint16_t port);

        void setAllowLocalIp(bool allow);

        void setPeers(const std::vector<PeerlistEntry> &peerList);

        void setPriorityNodes(const std::vector<NetworkAddress> &addresses);

        void setExclusiveNodes(const std::vector<NetworkAddress> &addresses);

        void setSeedNodes(const std::vector<NetworkAddress> &addresses);

        void setHideMyPort(bool hide);

        void setConfigFolder(const std::string &folder);

    private:
        std::string bindIp;
        uint16_t bindPort;
        uint16_t externalPort;
        bool allowLocalIp;
        std::vector<PeerlistEntry> peers;
        std::vector<NetworkAddress> priorityNodes;
        std::vector<NetworkAddress> exclusiveNodes;
        std::vector<NetworkAddress> seedNodes;
        bool hideMyPort;
        std::string configFolder;
        std::string p2pStateFilename;
        bool testnet;
    };

} //namespace nodetool
