// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <memory>

#include <NodeRpcProxy/NodeRpcProxy.h>

#include <WalletBackend/MultiThreadedDeque.h>
#include <WalletBackend/SynchronizationStatus.h>
#include <WalletBackend/RawData.h>

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

    private:
        void downloadBlocks();

        void findTransactionsInBlocks();

        void stop();

        void invalidateTransactions(uint64_t height);

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
        MultiThreadedDeque<RawBlock> m_blockProcessingQueue;

        /* The timestamp to start scanning downloading the full block data
           from */
        uint64_t m_startTimestamp;

        /* The private view key we use for decrypting transactions */
        Crypto::SecretKey m_privateViewKey;
};
