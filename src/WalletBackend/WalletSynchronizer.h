// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <memory>

#include <NodeRpcProxy/NodeRpcProxy.h>

class WalletSynchronizer
{
    public:
        /* Default constructor */
        WalletSynchronizer();

        /* Parameterized constructor */
        WalletSynchronizer(std::shared_ptr<CryptoNote::NodeRpcProxy> daemon);

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

    private:
        void sync();

        std::vector<Crypto::Hash> getBlockHashCheckpoints();

        void storeBlockHashCheckpoint(Crypto::Hash hash, uint32_t height);

        /* The daemon connection */
        std::shared_ptr<CryptoNote::NodeRpcProxy> m_daemon;

        /* The thread ID of the sync thread */
        std::thread m_workerThread;

        /* An atomic bool to signal if we should stop the sync thread */
        std::atomic<bool> m_workerThreadShouldStop;

        /* A mapping of block heights to hashes, used to know where to resume
           the sync progress from in the case of forked blocks */
        std::vector<Crypto::Hash> m_blockCheckpoints;

        /* A double ended queue for storing the 100 (or some other number) of
           last known block heights. We use this to store the sync progress,
           and resume cleanly when a fork occurs. We add the latest blocks
           to the front of the queue, and remove from the back of the queue,
           if we have reached capacity. */
        std::deque<Crypto::Hash> m_lastKnownBlockHeights;

        /* The size of the m_lastKnownBlockHeights container */
        static constexpr int LAST_KNOWN_BLOCK_HEIGHTS_SIZE = 100;

        /* Save a block checkpoint every BLOCK_CHECKPOINTS_INTERVAL blocks */
        static constexpr int BLOCK_CHECKPOINTS_INTERVAL = 5000;
};
