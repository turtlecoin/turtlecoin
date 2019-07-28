// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "VectorOutputStream.h"

namespace Common
{
    VectorOutputStream::VectorOutputStream(std::vector<uint8_t> &out): out(out) {}

    uint64_t VectorOutputStream::writeSome(const void *data, uint64_t size)
    {
        out.insert(out.end(), static_cast<const uint8_t *>(data), static_cast<const uint8_t *>(data) + size);
        return size;
    }

} // namespace Common
