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

void WalletSynchronizer::sync()
{
    /* Stores the results from the queryBlocks() call */
    std::vector<CryptoNote::BlockShortEntry> newBlocks;

    /* Hashes of the blocks we know */
    std::vector<Crypto::Hash> knownBlockHashes;

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

        m_daemon->queryBlocks(
            std::move(knownBlockHashes), startTimestamp, newBlocks,
            startHeight, callback
        );

        /* Check if the call succeeded (synchronously waiting on the future) */
        auto err = error.get();

        if (err)
        {
            std::cout << "Failed to query blocks: " << err << ", " << err.message() << std::endl;
        }
        else
        {
            std::cout << "Size: " << newBlocks.size() << std::endl;
            std::cout << "Start height: " << startHeight << std::endl << std::endl;
        }
    }
}
