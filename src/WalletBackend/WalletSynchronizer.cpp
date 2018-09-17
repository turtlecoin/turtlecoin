// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

////////////////////////////////////////
#include <WalletBackend/WalletSynchronizer.h>
////////////////////////////////////////

#include <Common/StringTools.h>

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
    m_shouldStop(false),
    m_startTimestamp(0)
{
}

/* Parameterized constructor */
WalletSynchronizer::WalletSynchronizer(std::shared_ptr<CryptoNote::NodeRpcProxy> daemon,
                                       uint64_t startTimestamp) :
    m_daemon(daemon),
    m_shouldStop(false),
    m_startTimestamp(startTimestamp)
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
    /* Stop any running threads */
    stop();

    m_daemon = std::move(old.m_daemon);

    m_blockDownloaderThread = std::move(old.m_blockDownloaderThread);
    m_transactionSynchronizerThread = std::move(old.m_transactionSynchronizerThread);

    m_shouldStop.store(old.m_shouldStop.load());

    m_blockDownloaderStatus = std::move(old.m_blockDownloaderStatus);
    m_transactionSynchronizerStatus = std::move(old.m_transactionSynchronizerStatus);

    m_startTimestamp = std::move(old.m_startTimestamp);

    return *this;
}

/* Deconstructor */
WalletSynchronizer::~WalletSynchronizer()
{
    stop();
}

/////////////////////
/* CLASS FUNCTIONS */
/////////////////////

/* Launch the worker thread in the background. It's safest to do this in a 
   seperate function, so everything in the constructor gets initialized,
   and if we do any inheritance, things don't go awry. */
void WalletSynchronizer::start()
{
    if (m_daemon == nullptr)
    {
        throw std::runtime_error("Daemon has not been initialized!");
    }

    m_blockDownloaderThread = std::thread(
        &WalletSynchronizer::downloadBlocks, this
    );

    m_transactionSynchronizerThread = std::thread(
        &WalletSynchronizer::findTransactionsInBlocks, this
    );
}

void WalletSynchronizer::stop()
{
    /* If either of the threads are running (and not detached) */
    if (m_blockDownloaderThread.joinable() || 
        m_transactionSynchronizerThread.joinable())
    {
        /* Tell the threads to stop */
        m_shouldStop.store(true);

        /* Wait for the block downloader thread to finish (if applicable) */
        if (m_blockDownloaderThread.joinable())
        {
            m_blockDownloaderThread.join();
        }

        /* Wait for the transaction synchronizer thread to finish (if applicable) */
        if (m_transactionSynchronizerThread.joinable())
        {
            m_transactionSynchronizerThread.join();
        }
    }
}

void WalletSynchronizer::findTransactionsInBlocks()
{
}

void WalletSynchronizer::downloadBlocks()
{
    /* Stores the results from the queryBlocks() call */
    std::vector<CryptoNote::BlockShortEntry> newBlocks;

    /* The start height of the returned blocks */
    uint32_t startHeight = 0;

    /* The timestamp to begin searching at */
    uint64_t startTimestamp = m_startTimestamp;

    std::promise<std::error_code> errorPromise;

    /* Once the function is complete, set the error value from the promise */
    auto callback = [&errorPromise](std::error_code e)
    {
        errorPromise.set_value(e);
    };

    /* While we haven't been told to stop */
    while (!m_shouldStop.load())
    {
        /* Re-assign promise (can't reuse) */
        errorPromise = std::promise<std::error_code>();

        /* Get the std::future */
        auto error = errorPromise.get_future();

        /* The block hashes to try begin syncing from */
        auto blockCheckpoints = m_blockDownloaderStatus.getBlockHashCheckpoints();

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
                uint32_t height = i + startHeight;

                m_blockDownloaderStatus.storeBlockHash(newBlocks[i].blockHash,
                                                       height);

                /* TODO: Push the actual data here, not the hash */
                m_blockProcessingQueue.push_front(newBlocks[i].blockHash);
            }

            std::cout << "Syncing blocks: " << startHeight << std::endl;

            /* Empty the vector so we're not re-iterating the old ones */
            newBlocks.clear();
        }
    }
}
