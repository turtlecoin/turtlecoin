// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <string>
#include "IInputStream.h"

namespace Common
{

    class StringInputStream : public IInputStream
    {
    public:
        StringInputStream(const std::string &in);

        size_t readSome(void *data, size_t size) override;

    private:
        const std::string &in;
        size_t offset;
    };

}
