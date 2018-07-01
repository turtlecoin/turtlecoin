// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <stdint.h>
#include <System/Dispatcher.h>
#include "HTTP/HttpRequest.h"
#include "HTTP/HttpResponse.h"
#include "Logging/ConsoleLogger.h"
#include "Logging/LoggerRef.h"
#include "Rpc/HttpClient.h"

#include "TestNode.h"

using namespace CryptoNote;

namespace Tests {
  class RPCTestNode : public TestNode {
  public:
    RPCTestNode(uint16_t port, System::Dispatcher& d);

    virtual bool startMining(size_t threadsCount, const std::string& address) override;
    virtual bool stopMining() override;
    virtual bool stopDaemon() override;
    virtual bool getBlockTemplate(const std::string& minerAddress, CryptoNote::BlockTemplate& blockTemplate, uint64_t& difficulty) override;
    virtual bool submitBlock(const std::string& block) override;
    virtual bool getTailBlockId(Crypto::Hash& tailBlockId) override;
    virtual bool makeINode(std::unique_ptr<CryptoNote::INode>& node) override;
    virtual uint64_t getLocalHeight() override;

    virtual ~RPCTestNode() { }

  private:
    Logging::ConsoleLogger m_log;
    Logging::LoggerRef m_logger;
    uint16_t m_rpcPort;
    System::Dispatcher& m_dispatcher;
    CryptoNote::HttpClient m_httpClient;
  };
}
