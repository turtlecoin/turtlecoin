// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

////////////////////////////////////////
#include <WalletBackend/WalletSynchronizer.h>
////////////////////////////////////////

#include <future>

//////////////////////////
/* NON MEMBER FUNCTIONS */
//////////////////////////

namespace {
} // namespace

///////////////////////////////////
/* CONSTRUCTORS / DECONSTRUCTORS */
///////////////////////////////////

/* Default constructor */
WalletSynchronizer::WalletSynchronizer() :
    m_workerThreadShouldStop(false)
{
}

/* Parameterized constructor */
WalletSynchronizer::WalletSynchronizer(std::shared_ptr<CryptoNote::NodeRpcProxy> daemon) :
    m_daemon(daemon),
    m_workerThreadShouldStop(false)
{
}

/* Move constructor */
WalletSynchronizer::WalletSynchronizer(WalletSynchronizer && old)
{
    /* Call the move assignment operator */
    *this = std::move(old);
}

/* Move assignment operator */
WalletSynchronizer & WalletSynchronizer::operator=(WalletSynchronizer && old)
{
    /* Join the current thread, if it exists */
    if (m_workerThread.joinable())
    {
        m_workerThread.join();
    }

    /* Assign the new thread */
    m_workerThread = std::move(old.m_workerThread);

    return *this;
}

/* Deconstructor */
WalletSynchronizer::~WalletSynchronizer()
{
    /* If the thread is running (and not detached) */
    if (m_workerThread.joinable())
    {
        /* Tell the worker thread to stop */
        m_workerThreadShouldStop.store(true);

        /* Wait for the worker thread to stop */
        m_workerThread.join();
    }
}

/////////////////////
/* CLASS FUNCTIONS */
/////////////////////

/* Launch the worker thread in the background. It's safest to do this in a 
   seperate function, so everything in the constructor gets initialized,
   and if we do any inheritance, things don't go awry. */
void WalletSynchronizer::start()
{
    m_workerThread = std::thread(&WalletSynchronizer::sync, this);
}

/* This returns a vector of hashes, used to be passed to queryBlocks(), to
   determine where to begin syncing from. We could just pass in the last known
   block hash, but if this block was on a forked chain, we would have to
   discard all our progres, and begin again from the genesis block.

   Instead, we store the last 100 block hashes we know about (since forks
   are most likely going to be quite shallow forks, usually 1 or 2 blocks max),
   and then we store one hash every 5000 blocks, in case we have a very
   deep fork.
   
   Note that the first items in this vector are the latest block. On the
   daemon side, it loops through the vector, looking for the hash in its
   database, then returns the height it found. So, if you put your earliest
   block at the start of the vector, you're just going to start syncing from
   that block every time. */
std::vector<Crypto::Hash> WalletSynchronizer::getBlockHashCheckpoints()
{
    std::vector<Crypto::Hash> result;

    /* Copy the contents of m_lastKnownBlockHeights to result, these are the
       last 100 known block hashes we have synced. For example, if the top
       block we know about is 110, this contains [110, 109, 108.. 10]. */
    std::copy(m_lastKnownBlockHeights.begin(), m_lastKnownBlockHeights.end(),
              back_inserter(result));

    /* Append the contents of m_blockCheckpoints to result, these are the
       checkpoints we make every 5k blocks in case of deep forks */
    std::copy(m_blockCheckpoints.begin(), m_blockCheckpoints.end(),
              back_inserter(result));

    return result;
}

void WalletSynchronizer::storeBlockHashCheckpoint(Crypto::Hash hash,
                                                  uint32_t height)
{
    /* If we're at a height interval, add a block checkpoint (to the front
       of the vector) */
    if (height % BLOCK_CHECKPOINTS_INTERVAL)
    {
        m_blockCheckpoints.insert(m_blockCheckpoints.begin(), hash);
    }

    /* Add the hash to the beginning of the queue */
    m_lastKnownBlockHeights.push_front(hash);

    /* If we're exceeding capacity, remove the last (oldest) hash */
    if (m_lastKnownBlockHeights.size() > LAST_KNOWN_BLOCK_HEIGHTS_SIZE)
    {
        m_lastKnownBlockHeights.pop_back();
    }
}

void WalletSynchronizer::sync()
{
    /* Stores the results from the queryBlocks() call */
    std::vector<CryptoNote::BlockShortEntry> newBlocks;

    /* The start height of the returned blocks */
    uint32_t startHeight = 0;

    /* The timestamp to begin searching at */
    uint64_t startTimestamp = 0;

    std::promise<std::error_code> errorPromise;

    /* Once the function is complete, set the error value from the promise */
    auto callback = [&errorPromise](std::error_code e)
    {
        errorPromise.set_value(e);
    };

    /* While we haven't been told to stop */
    while (!m_workerThreadShouldStop.load())
    {
        /* Re-assign promise (can't reuse) */
        errorPromise = std::promise<std::error_code>();

        /* Get the std::future */
        auto error = errorPromise.get_future();

        /* The block hashes to try begin syncing from */
        std::vector<Crypto::Hash> blockCheckpoints = getBlockHashCheckpoints();

        m_daemon->queryBlocks(
            std::move(blockCheckpoints), startTimestamp, newBlocks,
            startHeight, callback
        );

        /* Check if the call succeeded (synchronously waiting on the future)

           TODO: Add a wait_for() so if the node hangs up, we still exit
           correctly. (i.e., checking the m_workerThreadShouldStop flag) */
        auto err = error.get();

        if (err)
        {
            std::cout << "Failed to query blocks: " << err << ", "
                      << err.message() << std::endl;
        }
        else
        {
            for (size_t i = 0; i < newBlocks.size(); i++)
            {
                storeBlockHashCheckpoint(newBlocks[i].blockHash, i + startHeight);
            }

            std::cout << "Start height: " << startHeight << std::endl;

            /* Empty the vector so we're not re-iterating the old ones */
            newBlocks.clear();
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
