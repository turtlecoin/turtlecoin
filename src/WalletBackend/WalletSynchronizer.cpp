// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

/////////////////////////////////////////////
#include <WalletBackend/WalletSynchronizer.h>
/////////////////////////////////////////////

#include <Common/StringTools.h>

#include <future>

//////////////////////////
/* NON MEMBER FUNCTIONS */
//////////////////////////

namespace
{

    RawBlock trimBlockShortEntry(CryptoNote::BlockShortEntry b,
                                 uint64_t height)
    {
        /* The coinbase transaction, which is the miner reward. Has no inputs. */
        RawCoinbaseTransaction coinbaseTransaction = getRawCoinbaseTransaction(
            b.block.baseTransaction
        );

        /* Standard transactions */
        std::vector<RawTransaction> transactions;

        for (const auto &tx : b.txsShortInfo)
        {
            transactions.push_back(getRawTransaction(tx.txPrefix));
        }

        return RawBlock(coinbaseTransaction, transactions, height, b.blockHash);
    }

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
                                       uint64_t startTimestamp,
                                       Crypto::SecretKey privateViewKey) :
    m_daemon(daemon),
    m_shouldStop(false),
    m_startTimestamp(startTimestamp),
    m_privateViewKey(privateViewKey)
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

        /* Stop the block processing queue so the threads don't hang trying
           to push/pull from the queue */
        m_blockProcessingQueue.stop();

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
    while (!m_shouldStop.load())
    {
        RawBlock b = m_blockProcessingQueue.pop_back();

        /* Could have stopped between entering the loop and getting a block */
        if (m_shouldStop.load())
        {
            return;
        }

        std::cout << "Block " << b.blockHeight << " has a hash of " << Common::podToHex(b.blockHash) << std::endl;

        /* Make sure to do this at the end, once the transactions are fully
           processed! Otherwise, we could miss a transaction depending upon
           when we save */
        m_transactionSynchronizerStatus.storeBlockHash(b.blockHash, b.blockHeight);
    }
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
           correctly. (i.e., checking the m_shouldStop flag) */
        auto err = error.get();

        if (err)
        {
            std::cout << "Failed to query blocks: " << err << ", "
                      << err.message() << std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        else
        {
            for (size_t i = 0; i < newBlocks.size(); i++)
            {
                if (m_shouldStop.load())
                {
                    return;
                }

                uint32_t height = startHeight + i;

                RawBlock block = trimBlockShortEntry(newBlocks[i], height);

                /* The daemon sometimes serves us duplicate blocks ;___; */
                if (m_blockDownloaderStatus.haveSeenBlock(block.blockHash))
                {
                    continue;
                }

                m_blockDownloaderStatus.storeBlockHash(block.blockHash,
                                                       height);

                m_blockProcessingQueue.push_front(block);
            }

            /* Empty the vector so we're not re-iterating the old ones */
            newBlocks.clear();
        }
    }
}

