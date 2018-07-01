// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <ostream>
#include "IOutputStream.h"

namespace Common
{

    class StdOutputStream : public IOutputStream
    {
    public:
        StdOutputStream(std::ostream &out);

        StdOutputStream &operator=(const StdOutputStream &) = delete;

        size_t writeSome(const void *data, size_t size) override;

    private:
        std::ostream &out;
    };

}
