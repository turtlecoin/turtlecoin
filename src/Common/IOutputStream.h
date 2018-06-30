// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstddef>

namespace Common
{

    class IOutputStream
    {
    public:
        virtual ~IOutputStream()
        {}

        virtual size_t writeSome(const void *data, size_t size) = 0;
    };

}
