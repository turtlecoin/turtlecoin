// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <memory>

#include <Nigel/Nigel.h>

#include <SubWallets/SubWallets.h>

#include <WalletBackend/EventHandler.h>
#include <WalletBackend/ThreadSafeQueue.h>
#include <WalletBackend/SynchronizationStatus.h>

#include <WalletTypes.h>

/* Used to store the data we have accumulating when scanning a specific
   block. We can't add the items directly, because we may stop midway
   through. If so, we need to not add anything. */
struct BlockScanTmpInfo
{
    /* Transactions that belong to us */
    std::vector<WalletTypes::Transaction> transactionsToAdd;

    /* The corresponding inputs to the transactions, indexed by public key
       (i.e., the corresponding subwallet to add the input to) */
    std::vector<std::tuple<Crypto::PublicKey, WalletTypes::TransactionInput>> inputsToAdd;

    /* Need to mark these as spent so we don't include them later */
    std::vector<std::tuple<Crypto::PublicKey, Crypto::KeyImage>> keyImagesToMarkSpent;
};

class WalletSynchronizer
{
    public:
        //////////////////
        /* Constructors */
        //////////////////

        /* Default constructor */
        WalletSynchronizer();

        /* Parameterized constructor */
        WalletSynchronizer(
            const std::shared_ptr<Nigel> daemon,
            const uint64_t startTimestamp,
            const uint64_t startHeight,
            const Crypto::SecretKey privateViewKey,
            const std::shared_ptr<EventHandler> eventHandler);

        /* Delete the copy constructor */
        WalletSynchronizer(const WalletSynchronizer &) = delete;

        /* Delete the assignment operator */
        WalletSynchronizer & operator=(const WalletSynchronizer &) = delete;

        /* Move constructor */
        WalletSynchronizer(WalletSynchronizer && old);

        /* Move assignment operator */
        WalletSynchronizer & operator=(WalletSynchronizer && old);

        /* Deconstructor */
        ~WalletSynchronizer();

        /////////////////////////////
        /* Public member functions */
        /////////////////////////////

        void start();

        void stop();

        /* Converts the class to a json object */
        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const;

        /* Initializes the class from a json string */
        void fromJSON(const JSONObject &j);

        void initializeAfterLoad(
            const std::shared_ptr<Nigel> daemon,
            const std::shared_ptr<EventHandler> eventHandler);

        void reset(uint64_t startHeight);

        uint64_t getCurrentScanHeight() const;

        void swapNode(const std::shared_ptr<Nigel> daemon);

        void setSyncStart(const uint64_t startTimestamp, const uint64_t startHeight);

        /////////////////////////////
        /* Public member variables */
        /////////////////////////////

        /* The sub wallets (shared with the main class) */
        std::shared_ptr<SubWallets> m_subWallets;

    private:

        //////////////////////////////
        /* Private member functions */
        //////////////////////////////

        void monitorLockedTransactions();

        void downloadBlocks();

        void findTransactionsInBlocks();

        /* Remove transactions from this height and above, they occured
           on a forked chain */
        void removeForkedTransactions(uint64_t forkHeight);

        /* Process the transaction inputs to find transactions which we spent */
        uint64_t processTransactionInputs(
            const std::vector<CryptoNote::KeyInput> keyInputs,
            std::unordered_map<Crypto::PublicKey, int64_t> &transfers,
            const uint64_t blockHeight,
            BlockScanTmpInfo &blockScanInfo);

        /* Process the transaction outputs to find incoming transactions */
        std::tuple<bool, uint64_t> processTransactionOutputs(
            const WalletTypes::RawCoinbaseTransaction &tx,
            std::unordered_map<Crypto::PublicKey, int64_t> &transfers,
            const uint64_t blockHeight,
            BlockScanTmpInfo &blockScanInfo);

        /* Process a coinbase transaction to see if it belongs to us */
        BlockScanTmpInfo processCoinbaseTransaction(
            const WalletTypes::RawCoinbaseTransaction rawTX,
            const uint64_t blockTimestamp,
            const uint64_t blockHeight,
            BlockScanTmpInfo blockScanInfo);

        /* Process a transaction to see if it belongs to us */
        BlockScanTmpInfo processTransaction(
            const WalletTypes::RawTransaction rawTX,
            const uint64_t blockTimestamp,
            const uint64_t blockHeight,
            BlockScanTmpInfo blockScanInfo);

        std::vector<uint64_t> getGlobalIndexes(
            const uint64_t blockHeight,
            const Crypto::Hash transactionHash);

        //////////////////////////////
        /* Private member variables */
        //////////////////////////////

        /* The thread ID of the block downloader thread */
        std::thread m_blockDownloaderThread;

        /* The thread ID of the transaction synchronizer thread */
        std::thread m_transactionSynchronizerThread;

        /* The thread ID of the pool watcher thread */
        std::thread m_poolWatcherThread;

        /* An atomic bool to signal if we should stop the sync thread */
        std::atomic<bool> m_shouldStop;

        /* We have two threads, the block downloader thread (the producer),
           which grabs blocks from a daemon, and pushes them into the work
           queue, and the transaction syncher thread, which takes blocks from
           the work queue, and searches for transactions belonging to the
           user.
           
           We need to store the status that they are both at, since we need
           both to provide the last known block hashes to the node, to get
           the next newest blocks, and we need to be able to resume the sync
           progress from where we have decrypted transactions from, rather
           than simply where we have downloaded blocks to.
           
           If we stored the block download status, if the queue was not empty
           when closing the program, we could miss transactions which had
           been downloaded, but not processed. */
        SynchronizationStatus m_blockDownloaderStatus;

        SynchronizationStatus m_transactionSynchronizerStatus;

        /* Blocks to be processed are added to the front, and are removed
           from the back */
        std::shared_ptr<ThreadSafeQueue<WalletTypes::WalletBlockInfo>>
            m_blockProcessingQueue = std::make_shared<ThreadSafeQueue<WalletTypes::WalletBlockInfo>>();

        /* The timestamp to start scanning downloading block data from */
        uint64_t m_startTimestamp;

        /* The height to start downloading block data from */
        uint64_t m_startHeight;

        /* The private view key we use for decrypting transactions */
        Crypto::SecretKey m_privateViewKey;

        /* Used for firing events, such as onSynced() */
        std::shared_ptr<EventHandler> m_eventHandler;

        /* The daemon connection */
        std::shared_ptr<Nigel> m_daemon;

        /* Have we launched the pool watcher thread yet (we launched it when
           synced) */
        bool m_hasPoolWatcherThreadLaunched = false;
};
