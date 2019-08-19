// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "ScopeExit.h"

namespace Tools
{
    ScopeExit::ScopeExit(std::function<void()> &&handler): m_handler(std::move(handler)), m_cancelled(false) {}

    ScopeExit::~ScopeExit()
    {
        if (!m_cancelled)
        {
            m_handler();
        }
    }

    void ScopeExit::cancel()
    {
        m_cancelled = true;
    }

    void ScopeExit::resume()
    {
        m_cancelled = false;
    }

} // namespace Tools
