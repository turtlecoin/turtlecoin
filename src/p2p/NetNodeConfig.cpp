// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2019, The CyprusCoin Developers
//
// Please see the included LICENSE file for more information.

#include "NetNodeConfig.h"

#include "common/StringTools.h"

#include <common/Util.h>
#include <config/CryptoNoteConfig.h>
#include <crypto/random.h>

namespace CryptoNote
{
    namespace
    {
        bool parsePeerFromString(NetworkAddress &pe, const std::string &node_addr)
        {
            return Common::parseIpAddressAndPort(pe.ip, pe.port, node_addr);
        }

        bool parsePeersAndAddToNetworkContainer(
            const std::vector<std::string> peerList,
            std::vector<NetworkAddress> &container)
        {
            for (const std::string &peer : peerList)
            {
                NetworkAddress networkAddress = NetworkAddress();
                if (!parsePeerFromString(networkAddress, peer))
                {
                    return false;
                }
                container.push_back(networkAddress);
            }
            return true;
        }

        bool parsePeersAndAddToPeerListContainer(
            const std::vector<std::string> peerList,
            std::vector<PeerlistEntry> &container)
        {
            for (const std::string &peer : peerList)
            {
                PeerlistEntry peerListEntry = PeerlistEntry();
                peerListEntry.id = Random::randomValue<uint64_t>();
                if (!parsePeerFromString(peerListEntry.adr, peer))
                {
                    return false;
                }
                container.push_back(peerListEntry);
            }
            return true;
        }

    } // namespace

    NetNodeConfig::NetNodeConfig()
    {
        bindIp = "";
        bindPort = 0;
        externalPort = 0;
        allowLocalIp = false;
        hideMyPort = false;
        configFolder = Tools::getDefaultDataDirectory();
        p2pStateReset = false;
    }

    bool NetNodeConfig::init(
        const std::string interface,
        const int port,
        const int external,
        const bool localIp,
        const bool hidePort,
        const std::string dataDir,
        const std::vector<std::string> addPeers,
        const std::vector<std::string> addExclusiveNodes,
        const std::vector<std::string> addPriorityNodes,
        const std::vector<std::string> addSeedNodes,
        const bool p2pResetPeerState)
    {
        bindIp = interface;
        bindPort = port;
        externalPort = external;
        allowLocalIp = localIp;
        hideMyPort = hidePort;
        configFolder = dataDir;
        p2pStateFilename = CryptoNote::parameters::P2P_NET_DATA_FILENAME;
        p2pStateReset = p2pResetPeerState;

        if (!addPeers.empty())
        {
            if (!parsePeersAndAddToPeerListContainer(addPeers, peers))
            {
                return false;
            }
        }

        if (!addExclusiveNodes.empty())
        {
            if (!parsePeersAndAddToNetworkContainer(addExclusiveNodes, exclusiveNodes))
            {
                return false;
            }
        }

        if (!addPriorityNodes.empty())
        {
            if (!parsePeersAndAddToNetworkContainer(addPriorityNodes, priorityNodes))
            {
                return false;
            }
        }

        if (!addSeedNodes.empty())
        {
            if (!parsePeersAndAddToNetworkContainer(addSeedNodes, seedNodes))
            {
                return false;
            }
        }

        return true;
    }

    std::string NetNodeConfig::getP2pStateFilename() const
    {
        return p2pStateFilename;
    }

    bool NetNodeConfig::getP2pStateReset() const
    {
        return p2pStateReset;
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

} // namespace CryptoNote
