// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "ISerializer.h"
#include "JsonInputValueSerializer.h"

#include <common/IInputStream.h>

namespace CryptoNote
{
    class KVBinaryInputStreamSerializer : public JsonInputValueSerializer
    {
      public:
        KVBinaryInputStreamSerializer(Common::IInputStream &strm);

        virtual bool binary(void *value, uint64_t size, Common::StringView name) override;

        virtual bool binary(std::string &value, Common::StringView name) override;
    };

} // namespace CryptoNote
