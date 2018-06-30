// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <istream>
#include "IInputStream.h"

namespace Common
{

    class StdInputStream : public IInputStream
    {
    public:
        StdInputStream(std::istream &in);

        StdInputStream &operator=(const StdInputStream &) = delete;

        size_t readSome(void *data, size_t size) override;

    private:
        std::istream &in;
    };

}
