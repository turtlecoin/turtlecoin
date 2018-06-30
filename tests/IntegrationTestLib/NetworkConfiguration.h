// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Tests
{

    enum class NodeType
    {
        RPC,
        InProcess
    };

    struct TestNodeConfiguration
    {
        NodeType nodeType = NodeType::RPC;
        bool testnet = true;
        uint16_t p2pPort;
        uint16_t rpcPort;

        std::string dataDir;
        std::string blockchainLocation; // copy blockchain from this path
        std::string logFile;
        std::string daemonPath; // only for rpc node
        bool cleanupDataDir = true;

        std::vector<std::string> exclusiveNodes;

        std::string getP2pAddress() const
        {
            return "127.0.0.1:" + std::to_string(p2pPort);
        }
    };

}
