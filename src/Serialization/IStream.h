// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>
#include <memory>

namespace CryptoNote
{

    class IInputStream
    {
    public:
        virtual size_t read(char *data, size_t size) = 0;
    };

    class IOutputStream
    {
    public:
        virtual void write(const char *data, size_t size) = 0;
    };

}
