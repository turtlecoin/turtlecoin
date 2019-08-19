// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <WalletTypes.h>
#include <memory>
#include <nigel/Nigel.h>
#include <subwallets/SubWallets.h>
#include <utilities/ThreadSafeDeque.h>
#include <utilities/ThreadSafePriorityQueue.h>
#include <walletbackend/BlockDownloader.h>
#include <walletbackend/EventHandler.h>
#include <walletbackend/SynchronizationStatus.h>

typedef std::vector<std::tuple<Crypto::PublicKey, WalletTypes::TransactionInput>> BlockInputsAndOwners;

typedef std::tuple<WalletTypes::WalletBlockInfo, BlockInputsAndOwners, uint32_t> SemiProcessedBlock;

/* Used to store the data we have accumulating when scanning a specific
   block. We can't add the items directly, because we may stop midway
   through. If so, we need to not add anything. */
struct BlockScanTmpInfo
{
    /* Transactions that belong to us */
    std::vector<WalletTypes::Transaction> transactionsToAdd;

    /* The corresponding inputs to the transactions, indexed by public key
       (i.e., the corresponding subwallet to add the input to) */
    BlockInputsAndOwners inputsToAdd;

    /* Need to mark these as spent so we don't include them later */
    std::vector<std::tuple<Crypto::PublicKey, Crypto::KeyImage>> keyImagesToMarkSpent;
};

class OrderByArrivalIndex
{
  public:
    /* Ordering based on the arrival index of the blocks, not on the block
       height. This is needed to ensure correct handling of network forks. */
    bool operator()(SemiProcessedBlock a, SemiProcessedBlock b)
    {
        return std::get<2>(a) > std::get<2>(b);
    }
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
        const std::shared_ptr<EventHandler> eventHandler,
        unsigned int threadCount);

    /* Delete the copy constructor */
    WalletSynchronizer(const WalletSynchronizer &) = delete;

    /* Delete the assignment operator */
    WalletSynchronizer &operator=(const WalletSynchronizer &) = delete;

    /* Move constructor */
    WalletSynchronizer(WalletSynchronizer &&old);

    /* Move assignment operator */
    WalletSynchronizer &operator=(WalletSynchronizer &&old);

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
        const std::shared_ptr<EventHandler> eventHandler,
        unsigned int threadCount);

    void reset(uint64_t startHeight);

    uint64_t getCurrentScanHeight() const;

    void swapNode(const std::shared_ptr<Nigel> daemon);

    void setSyncStart(const uint64_t startTimestamp, const uint64_t startHeight);

    void setSubWallets(const std::shared_ptr<SubWallets> subWallets);

  private:
    //////////////////////////////
    /* Private member functions */
    //////////////////////////////

    void mainLoop();

    void blockProcessingThread();

    std::vector<std::tuple<Crypto::PublicKey, WalletTypes::TransactionInput>>
        processBlockOutputs(const WalletTypes::WalletBlockInfo &block) const;

    void completeBlockProcessing(
        const WalletTypes::WalletBlockInfo &block,
        const std::vector<std::tuple<Crypto::PublicKey, WalletTypes::TransactionInput>> &ourInputs);

    BlockScanTmpInfo processBlockTransactions(
        const WalletTypes::WalletBlockInfo &block,
        const std::vector<std::tuple<Crypto::PublicKey, WalletTypes::TransactionInput>> &inputs) const;

    std::optional<WalletTypes::Transaction> processCoinbaseTransaction(
        const WalletTypes::WalletBlockInfo &block,
        const std::vector<std::tuple<Crypto::PublicKey, WalletTypes::TransactionInput>> &inputs) const;

    std::tuple<std::optional<WalletTypes::Transaction>, std::vector<std::tuple<Crypto::PublicKey, Crypto::KeyImage>>>
        processTransaction(
            const WalletTypes::WalletBlockInfo &block,
            const std::vector<std::tuple<Crypto::PublicKey, WalletTypes::TransactionInput>> &inputs,
            const WalletTypes::RawTransaction &tx) const;

    std::vector<std::tuple<Crypto::PublicKey, WalletTypes::TransactionInput>>
        processTransactionOutputs(const WalletTypes::RawCoinbaseTransaction &rawTX, const uint64_t blockHeight) const;

    std::unordered_map<Crypto::Hash, std::vector<uint64_t>> getGlobalIndexes(const uint64_t blockHeight) const;

    void removeForkedTransactions(const uint64_t forkHeight);

    void checkLockedTransactions();

    //////////////////////////////
    /* Private member variables */
    //////////////////////////////

    /* The thread ID of the block downloader thread */
    std::thread m_syncThread;

    /* An atomic bool to signal if we should stop the sync thread */
    std::atomic<bool> m_shouldStop;

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

    BlockDownloader m_blockDownloader;

    /* The sub wallets (shared with the main class) */
    std::shared_ptr<SubWallets> m_subWallets;

    /* Stores blocks for processing by processing threads */
    ThreadSafeDeque<std::tuple<WalletTypes::WalletBlockInfo, uint32_t>> m_blockProcessingQueue;

    /* Synchronizes the child threads waiting for blocks to process
       and the parent pushing blocks in */
    std::condition_variable m_haveBlocksToProcess;

    /* Synchronizes the child threads pushing blocks into the priority
       queue and the parent thread waiting for them all to arrive */
    std::condition_variable m_haveProcessedBlocksToHandle;

    std::mutex m_mutex;

    /* yeah.... that's a thread safe queue, which holds a block, and it's
       corresponding inputs, and the subwallet public key that each input
       belongs to. The blocks which arrived earlier come at the front of
       the queue. */
    ThreadSafePriorityQueue<SemiProcessedBlock, OrderByArrivalIndex> m_processedBlocks;

    /* Amount of sync threads to run */
    unsigned int m_threadCount;

    /* Stores thread ids of the block output processing threads */
    std::vector<std::thread> m_syncThreads;
};
