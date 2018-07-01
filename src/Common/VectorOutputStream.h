// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>
#include <vector>
#include "IOutputStream.h"

namespace Common
{

    class VectorOutputStream : public IOutputStream
    {
    public:
        VectorOutputStream(std::vector<uint8_t> &out);

        VectorOutputStream &operator=(const VectorOutputStream &) = delete;

        size_t writeSome(const void *data, size_t size) override;

    private:
        std::vector<uint8_t> &out;
    };

}
