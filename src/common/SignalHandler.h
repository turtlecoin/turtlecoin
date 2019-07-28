// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <functional>

namespace Tools
{
    class SignalHandler
    {
      public:
        static bool install(std::function<void(void)> t);
    };
} // namespace Tools
