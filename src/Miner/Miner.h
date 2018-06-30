// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <atomic>
#include <thread>

#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/RemoteContext.h>

#include "CryptoNote.h"
#include "CryptoNoteCore/Difficulty.h"

#include "Logging/LoggerRef.h"

namespace CryptoNote
{

    struct BlockMiningParameters
    {
        BlockTemplate blockTemplate;
        Difficulty difficulty;
    };

    class Miner
    {
    public:
        Miner(System::Dispatcher &dispatcher, Logging::ILogger &logger);

        ~Miner();

        BlockTemplate mine(const BlockMiningParameters &blockMiningParameters, size_t threadCount);

        //NOTE! this is blocking method
        void stop();

    private:
        System::Dispatcher &m_dispatcher;
        System::Event m_miningStopped;

        enum class MiningState : uint8_t
        {
            MINING_STOPPED, BLOCK_FOUND, MINING_IN_PROGRESS
        };
        std::atomic<MiningState> m_state;

        std::vector<std::unique_ptr<System::RemoteContext<void>>> m_workers;

        BlockTemplate m_block;

        Logging::LoggerRef m_logger;

        void runWorkers(BlockMiningParameters blockMiningParameters, size_t threadCount);

        void workerFunc(const BlockTemplate &blockTemplate, Difficulty difficulty, uint32_t nonceStep);

        bool setStateBlockFound();
    };

} //namespace CryptoNote
