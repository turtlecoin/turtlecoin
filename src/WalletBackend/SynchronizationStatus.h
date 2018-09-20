// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoTypes.h"

#include <deque>

#include "json.hpp"

#include <vector>

using nlohmann::json;

class SynchronizationStatus
{
    public:
        void storeBlockHash(Crypto::Hash hash, uint64_t blockHeight);

        std::vector<Crypto::Hash> getBlockHashCheckpoints();

        json toJson() const;

        void fromJson(const json &j);

    private:
        /* A store of block hashes (later blocks first, i.e. block 2 comes
           before block 1) These are stored every 5000 blocks or so, used
           for knowing where to resume sync from. We store these in addition
           to the most 100 recent blocks, in the case of deep forks. */
        std::deque<Crypto::Hash> m_blockHashCheckpoints;

        /* A double ended queue of the 100 most recently synced block hashes,
           used for knowing where to begin syncing from. Newer hashes come
           before older ones, so block 2 is in front of block 1 in the list. */
        std::deque<Crypto::Hash> m_lastKnownBlockHashes;

        /* The last block height we are aware of */
        uint64_t m_lastKnownBlockHeight = 0;

        /* How large should the m_lastKnownBlockHashes container be */
        static constexpr uint32_t LAST_KNOWN_BLOCK_HASHES_SIZE = 100;

        /* Save a block hash checkpoint every BLOCK_HASH_CHECKPOINTS_INTERVAL
           blocks */
        static constexpr uint32_t BLOCK_HASH_CHECKPOINTS_INTERVAL = 5000;
};
