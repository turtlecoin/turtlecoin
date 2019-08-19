// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoNote.h"

#include <atomic>
#include <mutex>
#include <system/Dispatcher.h>
#include <system/Event.h>
#include <system/RemoteContext.h>
#include <thread>

namespace CryptoNote
{
    struct BlockMiningParameters
    {
        BlockTemplate blockTemplate;
        uint64_t difficulty;
    };

    class Miner
    {
      public:
        Miner(System::Dispatcher &dispatcher);

        BlockTemplate mine(const BlockMiningParameters &blockMiningParameters, size_t threadCount);

        uint64_t getHashCount();

        // NOTE! this is blocking method
        void stop();

      private:
        System::Dispatcher &m_dispatcher;

        System::Event m_miningStopped;

        enum class MiningState : uint8_t
        {
            MINING_STOPPED,
            BLOCK_FOUND,
            MINING_IN_PROGRESS
        };

        std::atomic<MiningState> m_state;

        std::vector<std::unique_ptr<System::RemoteContext<void>>> m_workers;

        BlockTemplate m_block;

        std::atomic<uint64_t> m_hash_count = 0;

        std::mutex m_hashes_mutex;

        void runWorkers(BlockMiningParameters blockMiningParameters, size_t threadCount);

        void workerFunc(const BlockTemplate &blockTemplate, uint64_t difficulty, uint32_t nonceStep);

        bool setStateBlockFound();

        void incrementHashCount();
    };

} // namespace CryptoNote
