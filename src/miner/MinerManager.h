// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "BlockchainMonitor.h"
#include "Miner.h"
#include "MinerEvent.h"
#include "MiningConfig.h"
#include "logging/LoggerRef.h"

#include <queue>
#include <system/ContextGroup.h>
#include <system/Event.h>

namespace System
{
    class Dispatcher;
}

namespace Miner
{
    class MinerManager
    {
      public:
        MinerManager(
            System::Dispatcher &dispatcher,
            const CryptoNote::MiningConfig &config,
            const std::shared_ptr<httplib::Client> httpClient);

        void start();

      private:
        System::ContextGroup m_contextGroup;

        CryptoNote::MiningConfig m_config;

        CryptoNote::Miner m_miner;

        BlockchainMonitor m_blockchainMonitor;

        System::Event m_eventOccurred;

        std::queue<MinerEvent> m_events;

        bool isRunning;

        CryptoNote::BlockTemplate m_minedBlock;

        uint64_t m_lastBlockTimestamp;

        std::shared_ptr<httplib::Client> m_httpClient = nullptr;

        void eventLoop();

        MinerEvent waitEvent();

        void pushEvent(MinerEvent &&event);

        void printHashRate();

        void startMining(const CryptoNote::BlockMiningParameters &params);

        void stopMining();

        void startBlockchainMonitoring();

        void stopBlockchainMonitoring();

        bool submitBlock(const CryptoNote::BlockTemplate &minedBlock);

        CryptoNote::BlockMiningParameters requestMiningParameters();

        void adjustBlockTemplate(CryptoNote::BlockTemplate &blockTemplate) const;
    };

} // namespace Miner
