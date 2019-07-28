// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IInputStream.h"

#include <istream>

namespace Common
{
    class StdInputStream : public IInputStream
    {
      public:
        StdInputStream(std::istream &in);

        StdInputStream &operator=(const StdInputStream &) = delete;

        uint64_t readSome(void *data, uint64_t size) override;

      private:
        std::istream &in;
    };

} // namespace Common
