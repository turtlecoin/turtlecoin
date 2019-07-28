// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
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
        virtual uint64_t read(char *data, uint64_t size) = 0;
    };

    class IOutputStream
    {
      public:
        virtual void write(const char *data, uint64_t size) = 0;
    };

} // namespace CryptoNote
