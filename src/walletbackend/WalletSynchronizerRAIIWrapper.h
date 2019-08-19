// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <walletbackend/WalletSynchronizer.h>

class WalletSynchronizerRAIIWrapper
{
  public:
    WalletSynchronizerRAIIWrapper(const std::shared_ptr<WalletSynchronizer> walletSynchronizer):
        m_walletSynchronizer(walletSynchronizer) {};

    template<typename T> auto pauseSynchronizerToRunFunction(T func)
    {
        /* Can only perform one operation with the synchronizer stopped at
           once */
        std::scoped_lock lock(m_mutex);

        /* Stop the synchronizer */
        if (m_walletSynchronizer != nullptr)
        {
            m_walletSynchronizer->stop();
        }

        /* Run the function, now safe */
        auto result = func();

        /* Start the synchronizer again */
        if (m_walletSynchronizer != nullptr)
        {
            m_walletSynchronizer->start();
        }

        /* Return the extracted value */
        return result;
    }

  private:
    std::shared_ptr<WalletSynchronizer> m_walletSynchronizer;

    std::mutex m_mutex;
};
