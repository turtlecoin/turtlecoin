// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

//////////////////
#include "Miner.h"
//////////////////

#include <common/CheckDifficulty.h>
#include <common/StringTools.h>
#include <crypto/crypto.h>
#include <crypto/random.h>
#include <iostream>
#include <miner/BlockUtilities.h>
#include <system/InterruptedException.h>
#include <utilities/ColouredMsg.h>

namespace CryptoNote
{
    Miner::Miner(System::Dispatcher &dispatcher):
        m_dispatcher(dispatcher),
        m_miningStopped(dispatcher),
        m_state(MiningState::MINING_STOPPED)
    {
    }

    BlockTemplate Miner::mine(const BlockMiningParameters &blockMiningParameters, size_t threadCount)
    {
        if (threadCount == 0)
        {
            throw std::runtime_error("Miner requires at least one thread");
        }

        if (m_state == MiningState::MINING_IN_PROGRESS)
        {
            throw std::runtime_error("Mining is already in progress");
        }

        m_state = MiningState::MINING_IN_PROGRESS;
        m_miningStopped.clear();

        runWorkers(blockMiningParameters, threadCount);

        if (m_state == MiningState::MINING_STOPPED)
        {
            throw System::InterruptedException();
        }

        return m_block;
    }

    void Miner::stop()
    {
        MiningState state = MiningState::MINING_IN_PROGRESS;

        if (m_state.compare_exchange_weak(state, MiningState::MINING_STOPPED))
        {
            m_miningStopped.wait();
            m_miningStopped.clear();
        }
    }

    void Miner::runWorkers(BlockMiningParameters blockMiningParameters, size_t threadCount)
    {
        std::cout << InformationMsg("Started mining for difficulty of ")
                  << InformationMsg(blockMiningParameters.difficulty) << InformationMsg(". Good luck! ;)\n");

        try
        {
            blockMiningParameters.blockTemplate.nonce = Random::randomValue<uint32_t>();

            for (size_t i = 0; i < threadCount; ++i)
            {
                m_workers.emplace_back(std::unique_ptr<System::RemoteContext<void>>(new System::RemoteContext<void>(
                    m_dispatcher,
                    std::bind(
                        &Miner::workerFunc,
                        this,
                        blockMiningParameters.blockTemplate,
                        blockMiningParameters.difficulty,
                        static_cast<uint32_t>(threadCount)))));

                blockMiningParameters.blockTemplate.nonce++;
            }

            m_workers.clear();
        }
        catch (const std::exception &e)
        {
            std::cout << WarningMsg("Error occured whilst mining: ") << WarningMsg(e.what()) << std::endl;

            m_state = MiningState::MINING_STOPPED;
        }

        m_miningStopped.set();
    }

    void Miner::workerFunc(const BlockTemplate &blockTemplate, uint64_t difficulty, uint32_t nonceStep)
    {
        try
        {
            BlockTemplate block = blockTemplate;

            while (m_state == MiningState::MINING_IN_PROGRESS)
            {
                Crypto::Hash hash = getBlockLongHash(block);

                if (check_hash(hash, difficulty))
                {
                    if (!setStateBlockFound())
                    {
                        return;
                    }

                    m_block = block;
                    return;
                }

                incrementHashCount();
                block.nonce += nonceStep;
            }
        }
        catch (const std::exception &e)
        {
            std::cout << WarningMsg("Error occured whilst mining: ") << WarningMsg(e.what()) << std::endl;

            m_state = MiningState::MINING_STOPPED;
        }
    }

    bool Miner::setStateBlockFound()
    {
        auto state = m_state.load();

        while (true)
        {
            switch (state)
            {
                case MiningState::BLOCK_FOUND:
                {
                    return false;
                }
                case MiningState::MINING_IN_PROGRESS:
                {
                    if (m_state.compare_exchange_weak(state, MiningState::BLOCK_FOUND))
                    {
                        return true;
                    }

                    break;
                }
                case MiningState::MINING_STOPPED:
                {
                    return false;
                }
                default:
                {
                    return false;
                }
            }
        }
    }

    void Miner::incrementHashCount()
    {
        m_hash_count++;
    }

    uint64_t Miner::getHashCount()
    {
        return m_hash_count.load();
    }

} // namespace CryptoNote
