// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <memory>

#include <NodeRpcProxy/NodeRpcProxy.h>

#include <WalletBackend/MultiThreadedDeque.h>
#include <WalletBackend/SubWallets.h>
#include <WalletBackend/SynchronizationStatus.h>

#include <WalletTypes.h>

class WalletSynchronizer
{
    public:
        /* Default constructor */
        WalletSynchronizer();

        /* Parameterized constructor */
        WalletSynchronizer(std::shared_ptr<CryptoNote::NodeRpcProxy> daemon,
                           uint64_t startTimestamp,
                           Crypto::SecretKey privateViewKey);

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

        void start();

        json toJson() const;

        void fromJson(const json &j);

        /* The daemon connection */
        std::shared_ptr<CryptoNote::NodeRpcProxy> m_daemon;

        /* The sub wallets (shared with the main class) */
        std::shared_ptr<SubWallets> m_subWallets;

    private:
        void downloadBlocks();

        void findTransactionsInBlocks();

        void stop();

        /* Invalidate transactions from this height and above, they occured
           on a forked chain */
        void invalidateTransactions(uint64_t height);

        /* Process the transaction inputs to find transactions which we spent */
        uint64_t processTransactionInputs(
            std::vector<CryptoNote::KeyInput> keyInputs,
            std::unordered_map<Crypto::PublicKey, int64_t> &transfers);

        /* Process the transaction outputs to find incoming transactions */
        std::tuple<bool, uint64_t> processTransactionOutputs(
            WalletTypes::RawTransaction tx,
            std::unordered_map<Crypto::PublicKey, int64_t> &transfers);

        /* Process the transaction outputs to find incoming transactions */
        std::tuple<bool, uint64_t> processTransactionOutputs(
            std::vector<WalletTypes::KeyOutput> keyOutputs,
            std::unordered_map<Crypto::PublicKey, int64_t> &transfers,
            Crypto::PublicKey txPublicKey);

        /* Process a coinbase transaction to see if it belongs to us */
        void processCoinbaseTransaction(
            WalletTypes::RawCoinbaseTransaction rawTX,
            uint64_t blockTimestamp,
            uint64_t blockHeight);

        /* Process a transaction to see if it belongs to us */
        void processTransaction(WalletTypes::RawTransaction rawTX,
                                uint64_t blockTimestamp,
                                uint64_t blockHeight);

        /* The thread ID of the block downloader thread */
        std::thread m_blockDownloaderThread;

        /* The thread ID of the transaction synchronizer thread */
        std::thread m_transactionSynchronizerThread;

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
        MultiThreadedDeque<WalletTypes::WalletBlockInfo> m_blockProcessingQueue;

        /* The timestamp to start scanning downloading the full block data
           from */
        uint64_t m_startTimestamp;

        /* The private view key we use for decrypting transactions */
        Crypto::SecretKey m_privateViewKey;
};
