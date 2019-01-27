// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

////////////////////////////////////////////////
#include <WalletBackend/SynchronizationStatus.h>
////////////////////////////////////////////////

#include <WalletBackend/Constants.h>

/////////////////////
/* CLASS FUNCTIONS */
/////////////////////

uint64_t SynchronizationStatus::getHeight() const
{
    return m_lastKnownBlockHeight;
}

void SynchronizationStatus::storeBlockHash(
    const Crypto::Hash hash,
    const uint64_t height)
{
    /* If it's not a fork and not the very first block */
    if (height > m_lastKnownBlockHeight && m_lastKnownBlockHeight != 0)
    {
        /* Height should be one more than previous height */
        if (height != m_lastKnownBlockHeight + 1)
        {
            std::stringstream stream;

            stream << "Blocks were missed in syncing process! Expected: "
                   << m_lastKnownBlockHeight + 1 << ", Received: "
                   << height << ".\nPossibly malicious daemon. Terminating.";

            throw std::runtime_error(stream.str());
        }
    }

    m_lastKnownBlockHeight = height;

    /* If we're at a checkpoint height, add the hash to the infrequent
       checkpoints (at the beginning of the queue) */
    if (height % Constants::BLOCK_HASH_CHECKPOINTS_INTERVAL == 0)
    {
        m_blockHashCheckpoints.push_front(hash);
    }

    m_lastKnownBlockHashes.push_front(hash);

    /* If we're exceeding capacity, remove the last (oldest) hash */
    if (m_lastKnownBlockHashes.size() > Constants::LAST_KNOWN_BLOCK_HASHES_SIZE)
    {
        m_lastKnownBlockHashes.pop_back();
    }
}

/* This returns a vector of hashes, used to be passed to queryBlocks(), to
   determine where to begin syncing from. We could just pass in the last known
   block hash, but if this block was on a forked chain, we would have to
   discard all our progress, and begin again from the genesis block.

   Instead, we store the last 100 block hashes we know about (since forks
   are most likely going to be quite shallow forks, usually 1 or 2 blocks max),
   and then we store one hash every 5000 blocks, in case we have a very
   deep fork.
   
   Note that the first items in this vector are the latest block. On the
   daemon side, it loops through the vector, looking for the hash in its
   database, then returns the height it found. So, if you put your earliest
   block at the start of the vector, you're just going to start syncing from
   that block every time. */
std::vector<Crypto::Hash> SynchronizationStatus::getBlockHashCheckpoints() const
{
    std::vector<Crypto::Hash> results;

    /* Copy the contents of m_lastKnownBlockHashes to result, these are the
       last 100 known block hashes we have synced. For example, if the top
       block we know about is 110, this contains [110, 109, 108.. 10]. */
    std::copy(m_lastKnownBlockHashes.begin(), m_lastKnownBlockHashes.end(),
              back_inserter(results));

    /* Append the contents of m_blockHashCheckpoints to result, these are the
       checkpoints we make every 5k blocks in case of deep forks */
    std::copy(m_blockHashCheckpoints.begin(), m_blockHashCheckpoints.end(),
              back_inserter(results));

    return results;
}

void SynchronizationStatus::fromJSON(const JSONObject &j)
{
    for (const auto &x : getArrayFromJSON(j, "blockHashCheckpoints"))
    {
        Crypto::Hash h;
        h.fromString(getStringFromJSONString(x));
        m_blockHashCheckpoints.push_back(h);
    }

    for (const auto &x : getArrayFromJSON(j, "lastKnownBlockHashes"))
    {
        Crypto::Hash h;
        h.fromString(getStringFromJSONString(x));
        m_lastKnownBlockHashes.push_back(h);
    }

    m_lastKnownBlockHeight = getUint64FromJSON(j, "lastKnownBlockHeight");
}

void SynchronizationStatus::toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
{
    writer.StartObject();

    writer.Key("blockHashCheckpoints");
    writer.StartArray();
    for (const auto hash : m_blockHashCheckpoints)
    {
        hash.toJSON(writer);
    }
    writer.EndArray();

    writer.Key("lastKnownBlockHashes");
    writer.StartArray();
    for (const auto hash : m_lastKnownBlockHashes)
    {
        hash.toJSON(writer);
    }
    writer.EndArray();

    writer.Key("lastKnownBlockHeight");
    writer.Uint(m_lastKnownBlockHeight);

    writer.EndObject();
}
