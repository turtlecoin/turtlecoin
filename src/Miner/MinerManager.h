// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <queue>

#include <System/ContextGroup.h>
#include <System/Event.h>

#include "BlockchainMonitor.h"
#include "Logging/LoggerRef.h"
#include "Miner.h"
#include "MinerEvent.h"
#include "MiningConfig.h"

namespace System
{
    class Dispatcher;
}

namespace Miner
{

    class MinerManager
    {
    public:
        MinerManager(System::Dispatcher &dispatcher, const CryptoNote::MiningConfig &config, Logging::ILogger &logger);

        ~MinerManager();

        void start();

    private:
        System::Dispatcher &m_dispatcher;
        Logging::LoggerRef m_logger;
        System::ContextGroup m_contextGroup;
        CryptoNote::MiningConfig m_config;
        CryptoNote::Miner m_miner;
        BlockchainMonitor m_blockchainMonitor;

        System::Event m_eventOccurred;
        System::Event m_httpEvent;
        std::queue<MinerEvent> m_events;

        CryptoNote::BlockTemplate m_minedBlock;

        uint64_t m_lastBlockTimestamp;

        void eventLoop();

        MinerEvent waitEvent();

        void pushEvent(MinerEvent &&event);

        void startMining(const CryptoNote::BlockMiningParameters &params);

        void stopMining();

        void startBlockchainMonitoring();

        void stopBlockchainMonitoring();

        bool
        submitBlock(const CryptoNote::BlockTemplate &minedBlock, const std::string &daemonHost, uint16_t daemonPort);

        CryptoNote::BlockMiningParameters
        requestMiningParameters(System::Dispatcher &dispatcher, const std::string &daemonHost, uint16_t daemonPort,
                                const std::string &miningAddress);

        void adjustBlockTemplate(CryptoNote::BlockTemplate &blockTemplate) const;
    };

} //namespace Miner
