// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <utilities/ThreadSafeQueue.h>
#include <walletbackend/WalletBackend.h>

class TransactionMonitor
{
  public:
    TransactionMonitor(const std::shared_ptr<WalletBackend> walletBackend):
        m_walletBackend(walletBackend),
        m_shouldStop(false)
    {
    }

    void start();

    void stop();

    std::shared_ptr<std::mutex> getMutex() const;

  private:
    std::atomic<bool> m_shouldStop;

    std::shared_ptr<WalletBackend> m_walletBackend;

    ThreadSafeQueue<WalletTypes::Transaction> m_queuedTransactions;

    std::shared_ptr<std::mutex> m_mutex = std::make_shared<std::mutex>();
};
