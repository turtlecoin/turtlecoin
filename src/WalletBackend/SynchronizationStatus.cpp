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

            /* TODO: Convert to log message */
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

std::deque<Crypto::Hash> SynchronizationStatus::getBlockCheckpoints() const
{
    return m_blockHashCheckpoints;
}

std::deque<Crypto::Hash> SynchronizationStatus::getRecentBlockHashes() const
{
    return m_lastKnownBlockHashes;
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
    writer.Uint64(m_lastKnownBlockHeight);

    writer.EndObject();
}
