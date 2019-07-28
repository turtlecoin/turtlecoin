// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "serialization/JsonInputStreamSerializer.h"

#include <ctype.h>
#include <exception>

namespace CryptoNote
{
    namespace
    {
        Common::JsonValue getJsonValueFromStreamHelper(std::istream &stream)
        {
            Common::JsonValue value;
            stream >> value;
            return value;
        }

    } // namespace

    JsonInputStreamSerializer::JsonInputStreamSerializer(std::istream &stream):
        JsonInputValueSerializer(getJsonValueFromStreamHelper(stream))
    {
    }

    JsonInputStreamSerializer::~JsonInputStreamSerializer() {}

} // namespace CryptoNote
