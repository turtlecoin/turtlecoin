// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <future>

namespace System
{

    namespace Detail
    {

        template<class T> using Future = std::future<T>;

        template<class T>
        Future<T> async(std::function<T()> &&operation)
        {
            return std::async(std::launch::async, std::move(operation));
        }

    }

}
