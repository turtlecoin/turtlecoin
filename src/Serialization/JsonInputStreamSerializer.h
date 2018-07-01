// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <iosfwd>
#include <string>
#include <vector>
#include "../Common/JsonValue.h"
#include "JsonInputValueSerializer.h"

namespace CryptoNote
{

//deserialization
    class JsonInputStreamSerializer : public JsonInputValueSerializer
    {
    public:
        JsonInputStreamSerializer(std::istream &stream);

        virtual ~JsonInputStreamSerializer();
    };

}
