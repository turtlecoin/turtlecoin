// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "VectorOutputStream.h"

namespace Common
{

    VectorOutputStream::VectorOutputStream(std::vector<uint8_t> &out) : out(out)
    {
    }

    size_t VectorOutputStream::writeSome(const void *data, size_t size)
    {
        out.insert(out.end(), static_cast<const uint8_t *>(data), static_cast<const uint8_t *>(data) + size);
        return size;
    }

}
