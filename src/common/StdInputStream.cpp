// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "StdInputStream.h"

namespace Common
{
    StdInputStream::StdInputStream(std::istream &in): in(in) {}

    uint64_t StdInputStream::readSome(void *data, uint64_t size)
    {
        in.read(static_cast<char *>(data), size);
        return in.gcount();
    }

} // namespace Common
