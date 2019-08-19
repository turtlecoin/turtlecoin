// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoTypes.h"
#include "JsonHelper.h"
#include "json.hpp"

#include <deque>
#include <vector>

using nlohmann::json;

class SynchronizationStatus
{
  public:
    /////////////////////////////
    /* Public member functions */
    /////////////////////////////

    void storeBlockHash(const Crypto::Hash hash, const uint64_t blockHeight);

    std::vector<Crypto::Hash> getBlockHashCheckpoints() const;

    std::deque<Crypto::Hash> getBlockCheckpoints() const;

    std::deque<Crypto::Hash> getRecentBlockHashes() const;

    /* Converts the class to a json object */
    void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const;

    /* Initializes the class from a json string */
    void fromJSON(const JSONObject &j);

    uint64_t getHeight() const;

  private:
    //////////////////////////////
    /* Private member variables */
    //////////////////////////////

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

    /* The last height we saved a block checkpoint at. Can't do every 5k
       since we skip blocks with coinbase tx scanning of. */
    uint64_t m_lastSavedCheckpointAt = 0;
};
