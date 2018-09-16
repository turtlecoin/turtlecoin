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

        std::shared_ptr<CryptoNote::NodeRpcProxy> m_daemon;

        std::thread m_workerThread;

        std::atomic<bool> m_workerThreadShouldStop;
};
