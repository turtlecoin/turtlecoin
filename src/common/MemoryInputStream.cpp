// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "MemoryInputStream.h"

#include <algorithm>
#include <cassert>
#include <cstring> // memcpy

namespace Common
{
    MemoryInputStream::MemoryInputStream(const void *buffer, uint64_t bufferSize):
        buffer(static_cast<const char *>(buffer)),
        bufferSize(bufferSize),
        position(0)
    {
    }

    bool MemoryInputStream::endOfStream() const
    {
        return position == bufferSize;
    }

    uint64_t MemoryInputStream::readSome(void *data, uint64_t size)
    {
        assert(position <= bufferSize);
        uint64_t readSize = std::min(size, bufferSize - position);

        if (readSize > 0)
        {
            memcpy(data, buffer + position, readSize);
            position += readSize;
        }

        return readSize;
    }

} // namespace Common
