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

        void mainLoop();

        std::vector<WalletTypes::WalletBlockInfo> downloadBlocks();

        std::vector<std::tuple<Crypto::PublicKey, WalletTypes::TransactionInput>> processBlockOutputs(
            const WalletTypes::WalletBlockInfo &block) const;

        void processBlock(const WalletTypes::WalletBlockInfo &block);

        BlockScanTmpInfo processBlockTransactions(
            const WalletTypes::WalletBlockInfo &block,
            const std::vector<std::tuple<Crypto::PublicKey, WalletTypes::TransactionInput>> &inputs) const;

        std::optional<WalletTypes::Transaction> processCoinbaseTransaction(
            const WalletTypes::WalletBlockInfo &block,
            const std::vector<std::tuple<Crypto::PublicKey, WalletTypes::TransactionInput>> &inputs) const;

        std::tuple<std::optional<WalletTypes::Transaction>, std::vector<std::tuple<Crypto::PublicKey, Crypto::KeyImage>>> processTransaction(
            const WalletTypes::WalletBlockInfo &block,
            const std::vector<std::tuple<Crypto::PublicKey, WalletTypes::TransactionInput>> &inputs,
            const WalletTypes::RawTransaction &tx) const;

        std::vector<std::tuple<Crypto::PublicKey, WalletTypes::TransactionInput>> processTransactionOutputs(
            const WalletTypes::RawCoinbaseTransaction &rawTX,
            const uint64_t blockHeight) const;

        std::unordered_map<Crypto::Hash, std::vector<uint64_t>> getGlobalIndexes(
            const uint64_t blockHeight) const;

        void removeForkedTransactions(const uint64_t forkHeight);

        void checkLockedTransactions();

        //////////////////////////////
        /* Private member variables */
        //////////////////////////////

        /* The thread ID of the block downloader thread */
        std::thread m_syncThread;

        /* An atomic bool to signal if we should stop the sync thread */
        std::atomic<bool> m_shouldStop;

        SynchronizationStatus m_syncStatus;

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
};
