// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

/////////////////////////////////////////////
#include <WalletBackend/WalletSynchronizer.h>
/////////////////////////////////////////////

#include <Common/StringTools.h>

#include <crypto/crypto.h>

#include <future>

#include <Utilities/Utilities.h>

#include <WalletBackend/Constants.h>

///////////////////////////////////
/* CONSTRUCTORS / DECONSTRUCTORS */
///////////////////////////////////

/* Default constructor */
WalletSynchronizer::WalletSynchronizer() :
    m_shouldStop(false),
    m_startTimestamp(0),
    m_startHeight(0)
{
}

/* Parameterized constructor */
WalletSynchronizer::WalletSynchronizer(
    const std::shared_ptr<Nigel> daemon,
    const uint64_t startHeight,
    const uint64_t startTimestamp,
    const Crypto::SecretKey privateViewKey,
    const std::shared_ptr<EventHandler> eventHandler) :

    m_daemon(daemon),
    m_shouldStop(false),
    m_startHeight(startHeight),
    m_startTimestamp(startTimestamp),
    m_privateViewKey(privateViewKey),
    m_eventHandler(eventHandler)
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

    m_blockDownloaderThread = std::move(old.m_blockDownloaderThread);
    m_transactionSynchronizerThread = std::move(old.m_transactionSynchronizerThread);
    m_poolWatcherThread = std::move(old.m_poolWatcherThread);

    m_blockDownloaderStatus = std::move(old.m_blockDownloaderStatus);
    m_transactionSynchronizerStatus = std::move(old.m_transactionSynchronizerStatus);

    m_startTimestamp = std::move(old.m_startTimestamp);
    m_startHeight = std::move(old.m_startHeight);

    m_privateViewKey = std::move(old.m_privateViewKey);

    m_eventHandler = std::move(old.m_eventHandler);

    m_daemon = std::move(old.m_daemon);

    m_hasPoolWatcherThreadLaunched = std::move(old.m_hasPoolWatcherThreadLaunched);

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
    /* Call stop first, so if we reassign any threads, they have been correctly
       stopped */
    stop();

    /* Reinit any vars which may have changed if we previously called stop() */
    m_shouldStop = false;

    m_hasPoolWatcherThreadLaunched = false;

    if (m_daemon == nullptr)
    {
        throw std::runtime_error("Daemon has not been initialized!");
    }

    /* Make sure to start the queue before the downloader, and the downloader
       before the synchronizer, to avoid data races */
    m_blockProcessingQueue->start();

    m_blockDownloaderThread = std::thread(
        &WalletSynchronizer::downloadBlocks, this
    );

    m_transactionSynchronizerThread = std::thread(
        &WalletSynchronizer::findTransactionsInBlocks, this
    );
}

void WalletSynchronizer::stop()
{
    /* Tell the threads to stop */
    m_shouldStop = true;
	
    /* Stop the block processing queue so the threads don't hang trying
       to push/pull from the queue */
    m_blockProcessingQueue->stop();
	
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
	
    /* Wait for the pool watcher thread to finish (if applicable) */
    if (m_poolWatcherThread.joinable())
    {
        m_poolWatcherThread.join();
    }
}

void WalletSynchronizer::reset(uint64_t startHeight)
{
    stop();

    /* Reset start height / timestamp */
    m_startHeight = startHeight;
    m_startTimestamp = 0;

    /* Empty the block processing queue (Start it in 'stopped' mode) */
    m_blockProcessingQueue = std::make_shared<ThreadSafeQueue<WalletTypes::WalletBlockInfo>>(false);

    /* Discard sync progress */
    m_blockDownloaderStatus = SynchronizationStatus();
    m_transactionSynchronizerStatus = SynchronizationStatus();

    /* Need to call start in your calling code - We don't call it here so
       you can schedule the start correctly */
}

/* Remove any transactions at this height or above, they were on a forked
   chain */
void WalletSynchronizer::removeForkedTransactions(const uint64_t forkHeight)
{
    m_subWallets->removeForkedTransactions(forkHeight);
}

/* Find inputs that belong to us (i.e., outgoing transactions) */
uint64_t WalletSynchronizer::processTransactionInputs(
    const std::vector<CryptoNote::KeyInput> keyInputs,
    std::unordered_map<Crypto::PublicKey, int64_t> &transfers,
    const uint64_t blockHeight,
    BlockScanTmpInfo &blockScanInfo)
{
    uint64_t sumOfInputs = 0;

    for (const auto keyInput : keyInputs)
    {
        sumOfInputs += keyInput.amount;
        
        /* Note: If we're using a view wallet, this just returns false */

        /* See if any of the sub wallets contain this key image. If they do,
           it means this keyInput is an outgoing transfer from that wallet.
           
           We grab the spendKey so we can index the transfers array and then
           notify the subwallets all at once */
        auto [found, publicSpendKey] = m_subWallets->getKeyImageOwner(
            keyInput.keyImage
        );

        if (found)
        {
            /* Take the amount off the current amount (If a key doesn't exist,
               it will default to zero, so this is just setting the value
               to the negative amount in that case */
            transfers[publicSpendKey] -= static_cast<int64_t>(keyInput.amount);

            blockScanInfo.keyImagesToMarkSpent.emplace_back(publicSpendKey, keyInput.keyImage);
        }
    }

    return sumOfInputs;
}

/* Find outputs that belong to us (i.e., incoming transactions) */
std::tuple<bool, uint64_t> WalletSynchronizer::processTransactionOutputs(
    const WalletTypes::RawCoinbaseTransaction &tx,
    std::unordered_map<Crypto::PublicKey, int64_t> &transfers,
    const uint64_t blockHeight,
    BlockScanTmpInfo &blockScanInfo)
{
    Crypto::KeyDerivation derivation;

    /* Generate the key derivation from the random tx public key, and our private
       view key */
    if (!Crypto::generate_key_derivation(tx.transactionPublicKey, m_privateViewKey,
                                         derivation))
    {
        return {false, 0};
    }

    /* The sum of all the outputs in the transaction */
    uint64_t sumOfOutputs = 0;

    std::vector<uint64_t> globalIndexes;

    const auto spendKeys = m_subWallets->m_publicSpendKeys;

    for (size_t outputIndex = 0; outputIndex < tx.keyOutputs.size(); outputIndex++)
    {
        const uint64_t amount = tx.keyOutputs[outputIndex].amount;

        /* Add the amount to the sum of outputs, used for calculating fee later */
        sumOfOutputs += amount;

        Crypto::PublicKey spendKey;

        /* Derive the spend key from the transaction, using the previous
           derivation */
        if (!Crypto::underive_public_key(
            derivation, outputIndex, tx.keyOutputs[outputIndex].key, spendKey))
        {
            /* Not our output */
            continue;
        }

        /* See if the derived spend key matches any of our spend keys */
        auto ourSpendKey = std::find(spendKeys.begin(), spendKeys.end(),
                                     spendKey);
        
        /* If it does, the transaction belongs to us */
        if (ourSpendKey != spendKeys.end())
        {
            /* Get the indexes, if we haven't already got them. (Don't need
               to get them if we're in a view wallet, since we can't spend.) */
            if (globalIndexes.empty() && !m_subWallets->isViewWallet())
            {
                globalIndexes = getGlobalIndexes(blockHeight, tx.hash);

                /* We are stopping */
                if (globalIndexes.empty())
                {
                    return {false, 0};
                }
            }

            /* Add the amount to the current amount (If a key doesn't exist,
               it will default to zero, so this is just setting the value
               to the amount in that case */
            transfers[*ourSpendKey] += amount;

            WalletTypes::TransactionInput input;

            input.amount = amount;
            input.blockHeight = blockHeight;
            input.transactionPublicKey = tx.transactionPublicKey;
            input.transactionIndex = outputIndex;

            /* We don't fetch global indexes if using a view wallet since we
               don't need the global index */
            if (m_subWallets->isViewWallet())
            {
                input.globalOutputIndex = 0;
            }
            else
            {
                input.globalOutputIndex = globalIndexes[outputIndex];
            }

            input.key = tx.keyOutputs[outputIndex].key;
            input.spendHeight = 0;
            input.unlockTime = tx.unlockTime;
            input.parentTransactionHash = tx.hash;

            /* Note: If we're using a view wallet, this just returns an
               uninitialized key image */

            /* We need to fill in the key image of the transaction input -
               we'll let the subwallet do this since we need the private spend
               key. We use the key images to detect outgoing transactions,
               and we use the transaction inputs to make transactions ourself */
            input.keyImage = m_subWallets->getTxInputKeyImage(
                *ourSpendKey, derivation, outputIndex, input
            );

            blockScanInfo.inputsToAdd.emplace_back(*ourSpendKey, input);
        }
    }

    return {true, sumOfOutputs};
}

/* When we get the global indexes, we pass in a range of blocks, to obscure
   which transactions we are interested in - the ones that belong to us.
   To do this, we get the global indexes for all transactions in a range.

   For example, if we want the global indexes for a transaction in block
   17, we get all the indexes from block 10 to block 20. */
std::vector<uint64_t> WalletSynchronizer::getGlobalIndexes(
    const uint64_t blockHeight,
    const Crypto::Hash transactionHash)
{
    uint64_t startHeight = Utilities::getLowerBound(
        blockHeight, Constants::GLOBAL_INDEXES_OBSCURITY
    );

    uint64_t endHeight = Utilities::getUpperBound(
        blockHeight, Constants::GLOBAL_INDEXES_OBSCURITY
    );

    while (true)
    {
        const auto [success, indexes] = m_daemon->getGlobalIndexesForRange(
            startHeight, endHeight
        );

        /* Need to get the indexes, or we can't continue... */
        if (!success || indexes.empty())
        {
            Utilities::sleepUnlessStopping(std::chrono::seconds(1), m_shouldStop);

            /* We don't store the latest transaction if we're stopping, so it's
               ok to be in an invalid state */
            if (m_shouldStop)
            {
                return {};
            }

            continue;
        }

        const auto it = indexes.find(transactionHash);

        /* There's no real way to recover from this. */
        if (it == indexes.end())
        {
            throw std::runtime_error("Could not get global indexes from daemon! Possibly faulty/malicious daemon.");
        }

        return it->second;
    }
}

BlockScanTmpInfo WalletSynchronizer::processCoinbaseTransaction(
    const WalletTypes::RawCoinbaseTransaction rawTX,
    const uint64_t blockTimestamp,
    const uint64_t blockHeight,
    BlockScanTmpInfo blockScanInfo)
{
    std::unordered_map<Crypto::PublicKey, int64_t> transfers;

    processTransactionOutputs(rawTX, transfers, blockHeight, blockScanInfo);

    /* Process any transactions we found belonging to us */
    if (!transfers.empty())
    {
        /* Coinbase transactions don't have a fee */
        const uint64_t fee = 0;

        /* Coinbase transactions can't have payment ID's */
        const std::string paymentID;

        const bool isCoinbaseTransaction = true;

        /* Form the actual transaction */
        WalletTypes::Transaction tx(
            transfers, rawTX.hash, fee, blockTimestamp, blockHeight, paymentID,
            rawTX.unlockTime, isCoinbaseTransaction
        );

        /* Store the transaction for adding later if everything else succeeds */
        blockScanInfo.transactionsToAdd.push_back(tx);
    }

    return blockScanInfo;
}

/* Find the inputs and outputs of a transaction that belong to us */
BlockScanTmpInfo WalletSynchronizer::processTransaction(
    const WalletTypes::RawTransaction rawTX,
    const uint64_t blockTimestamp,
    const uint64_t blockHeight,
    BlockScanTmpInfo blockScanInfo)
{
    std::unordered_map<Crypto::PublicKey, int64_t> transfers;

    /* Finds the sum of inputs, adds the amounts that belong to us to the
       transfers map */
    const uint64_t sumOfInputs = processTransactionInputs(
        rawTX.keyInputs, transfers, blockHeight, blockScanInfo
    );

    /* Finds the sum of outputs, adds the amounts that belong to us to the
       transfers map, and stores any key images that belong to us */
    const auto [success, sumOfOutputs] = processTransactionOutputs(
        rawTX, transfers, blockHeight, blockScanInfo
    );

    /* Failed to parse a key */
    if (!success)
    {
        return blockScanInfo;
    }

    /* Process any transactions we found belonging to us */
    if (!transfers.empty())
    {
        /* Fee is the difference between inputs and outputs */
        const uint64_t fee = sumOfInputs - sumOfOutputs;

        const bool isCoinbaseTransaction = false;

        /* Form the actual transaction */
        const WalletTypes::Transaction tx(
            transfers, rawTX.hash, fee, blockTimestamp, blockHeight,
            rawTX.paymentID, rawTX.unlockTime, isCoinbaseTransaction
        );

        /* Store the transaction for adding later if everything else succeeds */
        blockScanInfo.transactionsToAdd.push_back(tx);
    }

    return blockScanInfo;
}

void WalletSynchronizer::findTransactionsInBlocks()
{
    while (!m_shouldStop)
    {
        WalletTypes::WalletBlockInfo b = m_blockProcessingQueue->peek();		
		
        /* Could have stopped between entering the loop and getting a block */
        if (m_shouldStop)
        {
            return;
        }

        /* Chain forked, invalidate previous transactions */
        if (m_transactionSynchronizerStatus.getHeight() >= b.blockHeight)
        {
            removeForkedTransactions(b.blockHeight);
        }

        BlockScanTmpInfo blockScanInfo;

        /* Process the coinbase transaction */
        blockScanInfo = processCoinbaseTransaction(
            b.coinbaseTransaction, b.blockTimestamp, b.blockHeight, blockScanInfo
        );

        /* Process the rest of the transactions */
        for (const auto &tx : b.transactions)
        {
            blockScanInfo = processTransaction(
                tx, b.blockTimestamp, b.blockHeight, blockScanInfo
            );
        }

        /* Have to check if we're stopping above/here - else we could infinite
           loop in getGlobalIndexesForRange */
        if (m_shouldStop)
        {
            return;
        }

        for (const auto tx : blockScanInfo.transactionsToAdd)
        {
            m_subWallets->addTransaction(tx);
            m_eventHandler->onTransaction.fire(tx);
        }

        for (const auto [publicKey, input] : blockScanInfo.inputsToAdd)
        {
            m_subWallets->storeTransactionInput(publicKey, input);
        }

        /* The input has been spent, discard the key image so we
           don't double spend it */
        for (const auto [publicKey, keyImage] : blockScanInfo.keyImagesToMarkSpent)
        {
            m_subWallets->markInputAsSpent(keyImage, publicKey, b.blockHeight);
        }

        /* We need to delete the item from the queue AFTER processing it.
           Otherwise, if we are stopping, like above, the transaction may be
           half or not processed, but we have removed it from the queue, so
           it will be skipped. Only remove the tx from the queue if we have
           fully processed it. */
        m_blockProcessingQueue->deleteFront();

        /* Make sure to do this at the end, once the transactions are fully
           processed! Otherwise, we could miss a transaction depending upon
           when we save */
        m_transactionSynchronizerStatus.storeBlockHash(
            b.blockHash, b.blockHeight
        );

        if (b.blockHeight >= m_daemon->networkBlockCount())
        {
            m_eventHandler->onSynced.fire(b.blockHeight);

            /* We are synced, launch the pool watcher thread to watch for
               locked transactions being spent or returning to the wallet.
               No need to launch if we're using a view wallet - can't have
               locked transactions in the pool */
            if (!m_hasPoolWatcherThreadLaunched && !m_subWallets->isViewWallet())
            {
                m_poolWatcherThread = std::thread(
                    &WalletSynchronizer::monitorLockedTransactions, this
                );

                m_hasPoolWatcherThreadLaunched = true;
            }
        }
    }
}

void WalletSynchronizer::monitorLockedTransactions()
{
    while (!m_shouldStop)
    {
        /* Get the hashes of any locked tx's we have */
        const auto lockedTxHashes = m_subWallets->getLockedTransactionsHashes();

        if (lockedTxHashes.size() != 0)
        {
            /* Transactions that are in the pool - we'll query these again
               next time to see if they have moved */
            std::unordered_set<Crypto::Hash> transactionsInPool;

            /* Transactions that are in a block - don't need to do anything,
               when we get to the block they will be processed and unlocked. */
            std::unordered_set<Crypto::Hash> transactionsInBlock;

            /* Transactions that the daemon doesn't know about - returned to
               our wallet for timeout or other reason */
            std::unordered_set<Crypto::Hash> cancelledTransactions;

            /* Get the status of the locked transactions */
            bool success = m_daemon->getTransactionsStatus(
                lockedTxHashes, transactionsInPool, transactionsInBlock,
                cancelledTransactions
            );

            /* Couldn't get info from the daemon, try again later */
            if (!success)
            {
                Utilities::sleepUnlessStopping(std::chrono::seconds(15), m_shouldStop);

                continue;
            }

            /* If some transactions have been cancelled, remove them, and their
               inputs */
            if (cancelledTransactions.size() != 0)
            {
                m_subWallets->removeCancelledTransactions(cancelledTransactions);
            }
        }

        /* Sleep for 15 seconds, unless we're exiting */
        Utilities::sleepUnlessStopping(std::chrono::seconds(15), m_shouldStop);
    }
}

void WalletSynchronizer::downloadBlocks()
{
    /* While we haven't been told to stop */
    while (!m_shouldStop)
    {
        const uint64_t localDaemonBlockCount = m_daemon->localDaemonBlockCount();

        const uint64_t walletBlockCount = m_blockDownloaderStatus.getHeight();

        /* Local daemon has less blocks than the wallet:

        With the get wallet sync data call, we give a height or a timestamp to
        start at, and an array of block hashes of the last known blocks we
        know about.
        
        If the daemon can find the hashes, it returns the next one it knows
        about, so if we give a start height of 200,000, and a hash of
        block 300,000, it will return block 300,001 and above.
        
        This works well, since if the chain forks at 300,000, it won't have the
        hash of 300,000, so it will return the next hash we gave it,
        in this case probably 299,999.
        
        On the wallet side, we'll detect a block lower than our last known
        block, and handle the fork.

        However, if we're syncing our wallet with an unsynced daemon,
        lets say our wallet is at height 600,000, and the daemon is at 300,000.
        If our start height was at 200,000, then since it won't have any block
        hashes around 600,000, it will start returning blocks from
        200,000 and up, discarding our current progress.

        Therefore, we should wait until the local daemon has more blocks than
        us to prevent discarding sync data. */
        if (localDaemonBlockCount < walletBlockCount) 
        {
            Utilities::sleepUnlessStopping(std::chrono::seconds(5), m_shouldStop);
            continue;
        }

        /* The block hashes to try begin syncing from */
        auto blockCheckpoints = m_blockDownloaderStatus.getBlockHashCheckpoints();

        /* Blocks the thread for up to 10 secs */
        const auto [success, blocks] = m_daemon->getWalletSyncData(
            blockCheckpoints, m_startHeight, m_startTimestamp
        );

        /* If we get no blocks, we are fully synced.
           (Or timed out/failed to get blocks)
           Sleep a bit so we don't spam the daemon. */
        if (!success || blocks.empty())
        {
            Utilities::sleepUnlessStopping(std::chrono::seconds(5), m_shouldStop);
            continue;
        }

        /* Timestamp is transient and can change - block height is constant. */
        if (m_startTimestamp != 0)
        {
            m_startTimestamp = 0;
            m_startHeight = blocks.front().blockHeight;

            m_subWallets->convertSyncTimestampToHeight(m_startTimestamp, m_startHeight);
        }

        /* If checkpoints are empty, this is the first sync request. */
        if (blockCheckpoints.empty())
        {
            const uint64_t actualHeight = blocks.front().blockHeight;

            /* Only check if a timestamp isn't given */
            if (m_startTimestamp == 0)
            {
                /* The height we expect to get back from the daemon */
                if (actualHeight != m_startHeight)
                {
                    std::stringstream stream;

                    stream << "Received unexpected block height from daemon. "
                           << "Expected " << m_startHeight << ", got "
                           << actualHeight << ". Terminating.";

                    throw std::runtime_error(stream.str());
                }
            }
        }

        for (const auto &block : blocks)
        {
            /* Add the block to the queue for processing */
            bool success = m_blockProcessingQueue->push(block);

            /* Need to ensure we don't store that we've downloaded blocks we
               haven't. If we're stopping (this occurs on startup, sometimes)
               we could push empty blocks, and some blocks get skipped, on
               the block processer side */
            if (!success)
            {
                return;
            }

            /* Store that we've downloaded the block */
            m_blockDownloaderStatus.storeBlockHash(block.blockHash,
                                                   block.blockHeight);
        }
    }
}

void WalletSynchronizer::initializeAfterLoad(
    const std::shared_ptr<Nigel> daemon,
    const std::shared_ptr<EventHandler> eventHandler)
{
    m_daemon = daemon;
    m_eventHandler = eventHandler;
}

uint64_t WalletSynchronizer::getCurrentScanHeight() const
{
    return m_transactionSynchronizerStatus.getHeight();
}

void WalletSynchronizer::swapNode(const std::shared_ptr<Nigel> daemon)
{
    m_daemon = daemon;
}

void WalletSynchronizer::fromJSON(const JSONObject &j)
{
    m_transactionSynchronizerStatus.fromJSON(getObjectFromJSON(j, "transactionSynchronizerStatus"));

    m_startTimestamp = getUint64FromJSON(j, "startTimestamp");

    m_startHeight = getUint64FromJSON(j, "startHeight");

    m_privateViewKey.fromString(getStringFromJSON(j, "privateViewKey"));
}

void WalletSynchronizer::toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
{
    writer.StartObject();

    writer.Key("transactionSynchronizerStatus");
    m_transactionSynchronizerStatus.toJSON(writer);

    writer.Key("startTimestamp");
    writer.Uint(m_startTimestamp);

    writer.Key("startHeight");
    writer.Uint(m_startHeight);

    writer.Key("privateViewKey");
    m_privateViewKey.toJSON(writer);

    writer.EndObject();
}
