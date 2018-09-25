// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

/////////////////////////////////////////////
#include <WalletBackend/WalletSynchronizer.h>
/////////////////////////////////////////////

#include <Common/StringTools.h>

#include <crypto/crypto.h>

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

    std::tuple<bool, Crypto::PublicKey>
        getPubKeyFromExtra(std::vector<uint8_t> extra)
    {
        Crypto::PublicKey publicKey;

        const int pubKeySize = 32;

        for (size_t i = 0; i < extra.size(); i++)
        {
            /* If the following data is the transaction public key, this is
               indicated by the preceding value being 0x01. */
            if (extra[i] == 0x01)
            {
                /* The amount of data remaining in the vector (minus one because
                   we start reading the public key from the next character) */
                size_t dataRemaining = extra.size() - i - 1;

                /* We need to check that there is enough space following the tag,
                   as someone could just pop a random 0x01 in there and make our
                   code mess up */
                if (dataRemaining < pubKeySize)
                {
                    return std::make_tuple(false, publicKey);
                }

                const auto dataBegin = extra.begin() + i + 1;
                const auto dataEnd = dataBegin + pubKeySize;

                /* Copy the data from the vector to the array */
                std::copy(dataBegin, dataEnd, std::begin(publicKey.data));

                return std::make_tuple(true, publicKey);
            }
        }

        /* Couldn't find the tag */
        return std::make_tuple(false, publicKey);
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
WalletSynchronizer::WalletSynchronizer(
    std::shared_ptr<CryptoNote::NodeRpcProxy> daemon, uint64_t startTimestamp,
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

    m_privateViewKey = std::move(old.m_privateViewKey);

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

/* Remove any transactions at this height or above, they were on a forked
   chain */
void WalletSynchronizer::invalidateTransactions(uint64_t height)
{
}

void WalletSynchronizer::processCoinbaseTransaction(RawCoinbaseTransaction tx)
{
}

void WalletSynchronizer::processTransaction(RawTransaction tx)
{
    Crypto::PublicKey txPublicKey;

    bool success;

    std::tie(success, txPublicKey) = getPubKeyFromExtra(tx.extra);

    if (!success)
    {
        return;
    }

    Crypto::KeyDerivation derivation;

    /* Generate the key derivation from the random tx public key, and our private
       view key */
    if (!Crypto::generate_key_derivation(txPublicKey, m_privateViewKey, derivation))
    {
        return;
    }

    /* TODO: Input is a uint64_t, but we store it as an int64_t so it can be
       negative - need to handle overflow */
    std::unordered_map<Crypto::PublicKey, int64_t> transfers;

    for (size_t outputIndex = 0; outputIndex < tx.keyOutputs.size(); outputIndex++)
    {
        Crypto::PublicKey spendKey;

        /* Derive the spend key from the transaction, using the previous
           derivation */
        Crypto::underive_public_key(
            derivation, outputIndex, tx.keyOutputs[outputIndex].key, spendKey
        );

        const auto spendKeys = m_subWallets->m_publicSpendKeys;

        /* See if the derived spend key matches any of our spend keys */
        auto ourSpendKey = std::find(spendKeys.begin(), spendKeys.end(), spendKey);
        
        /* If it does, the transaction belongs to us */
        if (ourSpendKey != spendKeys.end())
        {
            /* Add the amount to the current amount (If a key doesn't exist,
               it will default to zero, so this is just setting the value
               to the amount in that case */
            transfers[*ourSpendKey] += tx.keyOutputs[outputIndex].amount;

            /* Next we get the key image for this transaction. We store these,
               and thus can then detect when an outgoing transaction is made
               by us. We need a private spend key for this. 
               
               If the wallet this spend key belongs to is a view wallet, this
               won't do anything! */
            m_subWallets->generateAndStoreKeyImage(
                *ourSpendKey, derivation, outputIndex
            );
        }
    }

    for (const auto keyInput : tx.keyInputs)
    {
        Crypto::PublicKey publicSpendKey;

        bool found;

        /* See if any of the sub wallets contain this key image. If they do,
           it means this keyInput is an outgoing transfer from that wallet.
           
           We grab the spendKey so we can index the transfers array and then
           notify the subwallets all at once */
        std::tie(found, publicSpendKey) = m_subWallets->getKeyImageOwner(
            keyInput.keyImage
        );

        if (found)
        {
            /* Take the amount off the current amount (If a key doesn't exist,
               it will default to zero, so this is just setting the value
               to the negative amount in that case */
            transfers[publicSpendKey] -= keyInput.amount;
        }
    }

    /* Process any transactions we found belonging to us */
    if (!transfers.empty())
    {
        m_subWallets->addTransfers(transfers);
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

        /* Chain forked, invalidate previous transactions */
        if (m_transactionSynchronizerStatus.getHeight() >= b.blockHeight)
        {
            invalidateTransactions(b.blockHeight);
        }

        /* Process the coinbase transaction */
        processCoinbaseTransaction(b.coinbaseTransaction);

        /* Process the rest of the transactions */
        for (const auto &tx : b.transactions)
        {
            processTransaction(tx);
        }

        /* Make sure to do this at the end, once the transactions are fully
           processed! Otherwise, we could miss a transaction depending upon
           when we save */
        m_transactionSynchronizerStatus.storeBlockHash(
            b.blockHash, b.blockHeight
        );
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

        while (true)
        {
            /* Don't hang if the daemon doesn't respond */
            auto status = error.wait_for(std::chrono::milliseconds(50));

            if (m_shouldStop.load())
            {
                return;
            }
            
            /* queryBlocks() has returned */
            if (status == std::future_status::ready)
            {
                break;
            }
        }

        auto err = error.get();

        if (err)
        {
            std::cout << "Failed to query blocks: " << err << ", "
                      << err.message() << std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        else
        {
            std::cout << "Syncing blocks: " << startHeight << std::endl;

            /* Iterating through with a standard loop makes it a bit easier
               to get the height of the block */
            for (size_t i = 0; i < newBlocks.size(); i++)
            {
                if (m_shouldStop.load())
                {
                    return;
                }

                uint32_t height = startHeight + i;

                /* Trim the info we get from the daemon down */
                RawBlock block = trimBlockShortEntry(newBlocks[i], height);

                /* The daemon sometimes serves us duplicate blocks ;___; */
                if (m_blockDownloaderStatus.haveSeenBlock(block.blockHash))
                {
                    /* If we only get the one (dupe) block then we are fully
                       synced. Sleep a bit before asking for another block.
                       Not too long - don't want to block shutdown */
                    if (newBlocks.size() == 1)
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }

                    continue;
                }

                /* Store that we've downloaded the block */
                m_blockDownloaderStatus.storeBlockHash(block.blockHash,
                                                       height);

                /* Add the block to the queue for processing */
                m_blockProcessingQueue.push_front(block);
            }

            /* Empty the vector so we're not re-iterating the old ones */
            newBlocks.clear();
        }
    }
}
