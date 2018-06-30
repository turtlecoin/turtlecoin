// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <atomic>

namespace unit_test
{
    class call_counter
    {
    public:
        call_counter()
                : m_counter(0)
        {
        }

        void inc() volatile
        {
            // memory_order_relaxed is enough for call counter
            m_counter.fetch_add(1, std::memory_order_relaxed);
        }

        size_t get() volatile const
        {
            return m_counter.load(std::memory_order_relaxed);
        }

        void reset() volatile
        {
            m_counter.store(0, std::memory_order_relaxed);
        }

    private:
        std::atomic<size_t> m_counter;
    };
}
