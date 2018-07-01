// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "NetworkConfiguration.h"
#include "Process.h"
#include "TestNode.h"

namespace System {
class Dispatcher;
}

namespace CryptoNote {
class Currency;
}

namespace Tests {

enum class Topology {
  Ring,
  Line,
  Star
};


class TestNetworkBuilder {
public:

  TestNetworkBuilder(size_t nodeCount, Topology topology = Topology::Line, uint16_t rpcBasePort = 9200, uint16_t p2pBasePort = 9000);

  TestNetworkBuilder& setDataDirectory(const std::string& dataDir);
  TestNetworkBuilder& setBlockchain(const std::string& blockchainDir);
  TestNetworkBuilder& setTestnet(bool isTestnet);

  std::vector<TestNodeConfiguration> build();

private:

  TestNodeConfiguration buildNodeConfiguration(size_t index);

  uint16_t rpcBasePort;
  uint16_t p2pBasePort;
  Topology topology;
  size_t nodeCount;
  std::string baseDataDir;
  std::string blockchainLocation;
  bool testnet;
};


class TestNetwork {

public:

  TestNetwork(System::Dispatcher& dispatcher, const CryptoNote::Currency& currency);

  void addNodes(const std::vector<TestNodeConfiguration>& nodes);
  void addNode(const TestNodeConfiguration& cfg);
  void waitNodesReady();
  void shutdown();

  TestNode& getNode(size_t index);

private:

  std::unique_ptr<TestNode> startDaemon(const TestNodeConfiguration& cfg);

  std::vector<std::pair<std::unique_ptr<TestNode>, TestNodeConfiguration>> nodes;
  System::Dispatcher& m_dispatcher;
  const CryptoNote::Currency& m_currency;
  std::vector<Process> m_daemons;
};

}
