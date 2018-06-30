// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <Common/IInputStream.h>
#include "ISerializer.h"
#include "JsonInputValueSerializer.h"

namespace CryptoNote
{

    class KVBinaryInputStreamSerializer : public JsonInputValueSerializer
    {
    public:
        KVBinaryInputStreamSerializer(Common::IInputStream &strm);

        virtual bool binary(void *value, size_t size, Common::StringView name) override;

        virtual bool binary(std::string &value, Common::StringView name) override;
    };

}
