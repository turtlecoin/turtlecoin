// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstddef>
#include <cstdint>

namespace Common
{
    class IOutputStream
    {
      public:
        virtual ~IOutputStream() {}

        virtual uint64_t writeSome(const void *data, uint64_t size) = 0;
    };

} // namespace Common
