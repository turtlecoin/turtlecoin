// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "StdOutputStream.h"

namespace Common
{

    StdOutputStream::StdOutputStream(std::ostream &out) : out(out)
    {
    }

    size_t StdOutputStream::writeSome(const void *data, size_t size)
    {
        out.write(static_cast<const char *>(data), size);
        if (out.bad())
        {
            return 0;
        }

        return size;
    }

}
