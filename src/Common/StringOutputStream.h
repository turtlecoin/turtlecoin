// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <string>
#include "IOutputStream.h"

namespace Common
{

    class StringOutputStream : public IOutputStream
    {
    public:
        StringOutputStream(std::string &out);

        size_t writeSome(const void *data, size_t size) override;

    private:
        std::string &out;
    };

}
