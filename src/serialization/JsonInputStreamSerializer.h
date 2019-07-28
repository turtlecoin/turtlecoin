// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "../common/JsonValue.h"
#include "JsonInputValueSerializer.h"

#include <iosfwd>
#include <string>
#include <vector>

namespace CryptoNote
{
    // deserialization
    class JsonInputStreamSerializer : public JsonInputValueSerializer
    {
      public:
        JsonInputStreamSerializer(std::istream &stream);

        virtual ~JsonInputStreamSerializer();
    };

} // namespace CryptoNote
