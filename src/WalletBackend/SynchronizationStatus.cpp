// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.


////////////////////////////////////////////////
#include <WalletBackend/SynchronizationStatus.h>
////////////////////////////////////////////////

//////////////////////////
/* NON MEMBER FUNCTIONS */
//////////////////////////

namespace {
} // namespace

///////////////////////////////////
/* CONSTRUCTORS / DECONSTRUCTORS */
///////////////////////////////////

/////////////////////
/* CLASS FUNCTIONS */
/////////////////////

void SynchronizationStatus::storeBlockHash(Crypto::Hash hash, uint64_t height)
{
    m_lastKnownBlockHeight = height;

    /* If we're at a checkpoint height, add the hash to the infrequent
       checkpoints (at the beginning of the queue) */
    if (height % BLOCK_HASH_CHECKPOINTS_INTERVAL)
    {
        m_blockHashCheckpoints.push_front(hash);
    }

    m_lastKnownBlockHashes.push_front(hash);

    /* If we're exceeding capacity, remove the last (oldest) hash */
    if (m_lastKnownBlockHashes.size() > LAST_KNOWN_BLOCK_HASHES_SIZE)
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
std::vector<Crypto::Hash> SynchronizationStatus::getBlockHashCheckpoints()
{
    std::vector<Crypto::Hash> result;

    /* Copy the contents of m_lastKnownBlockHashes to result, these are the
       last 100 known block hashes we have synced. For example, if the top
       block we know about is 110, this contains [110, 109, 108.. 10]. */
    std::copy(m_lastKnownBlockHashes.begin(), m_lastKnownBlockHashes.end(),
              back_inserter(result));

    /* Append the contents of m_blockHashCheckpoints to result, these are the
       checkpoints we make every 5k blocks in case of deep forks */
    std::copy(m_blockHashCheckpoints.begin(), m_blockHashCheckpoints.end(),
              back_inserter(result));

    return result;
}
