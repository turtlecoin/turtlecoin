// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "TestNode.h"
#include "NetworkConfiguration.h"

#include <future>
#include <memory>
#include <thread>

#include <System/Dispatcher.h>


namespace CryptoNote
{
    class Core;

    class CryptoNoteProtocolHandler;

    class NodeServer;

    class Currency;
}

namespace Tests
{

    class InProcTestNode : public TestNode
    {
    public:
        InProcTestNode(const TestNodeConfiguration &cfg, const CryptoNote::Currency &currency, System::Dispatcher &d);

        ~InProcTestNode();

        virtual bool startMining(size_t threadsCount, const std::string &address) override;

        virtual bool stopMining() override;

        virtual bool stopDaemon() override;

        virtual bool getBlockTemplate(const std::string &minerAddress, CryptoNote::BlockTemplate &blockTemplate,
                                      uint64_t &difficulty) override;

        virtual bool submitBlock(const std::string &block) override;

        virtual bool getTailBlockId(Crypto::Hash &tailBlockId) override;

        virtual bool makeINode(std::unique_ptr<CryptoNote::INode> &node) override;

        virtual uint64_t getLocalHeight() override;

    private:

        void workerThread(std::promise<std::string> &initPromise);

        System::Dispatcher &dispatcher;
        std::unique_ptr<CryptoNote::Core> core;
        std::unique_ptr<CryptoNote::CryptoNoteProtocolHandler> protocol;
        std::unique_ptr<CryptoNote::NodeServer> p2pNode;

        std::thread m_thread;
        const CryptoNote::Currency &m_currency;
        TestNodeConfiguration m_cfg;
    };

}
