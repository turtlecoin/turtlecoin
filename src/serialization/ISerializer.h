// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <common/StringView.h>
#include <cstdint>
#include <string>

namespace CryptoNote
{
    class ISerializer
    {
      public:
        enum SerializerType
        {
            INPUT,
            OUTPUT
        };

        virtual ~ISerializer() {}

        virtual SerializerType type() const = 0;

        virtual bool beginObject(Common::StringView name) = 0;

        virtual void endObject() = 0;

        virtual bool beginArray(uint64_t &size, Common::StringView name) = 0;

        virtual void endArray() = 0;

        virtual bool operator()(uint8_t &value, Common::StringView name) = 0;

        virtual bool operator()(int16_t &value, Common::StringView name) = 0;

        virtual bool operator()(uint16_t &value, Common::StringView name) = 0;

        virtual bool operator()(int32_t &value, Common::StringView name) = 0;

        virtual bool operator()(uint32_t &value, Common::StringView name) = 0;

        virtual bool operator()(int64_t &value, Common::StringView name) = 0;

        virtual bool operator()(uint64_t &value, Common::StringView name) = 0;

        virtual bool operator()(double &value, Common::StringView name) = 0;

        virtual bool operator()(bool &value, Common::StringView name) = 0;

        virtual bool operator()(std::string &value, Common::StringView name) = 0;

        // read/write binary block
        virtual bool binary(void *value, uint64_t size, Common::StringView name) = 0;

        virtual bool binary(std::string &value, Common::StringView name) = 0;

        template<typename T> bool operator()(T &value, Common::StringView name);
    };

    template<typename T> bool ISerializer::operator()(T &value, Common::StringView name)
    {
        return serialize(value, name, *this);
    }

    template<typename T> bool serialize(T &value, Common::StringView name, ISerializer &serializer)
    {
        if (!serializer.beginObject(name))
        {
            return false;
        }

        serialize(value, serializer);
        serializer.endObject();
        return true;
    }

    /* WARNING: If you get a compiler error pointing to this line, when serializing
       a uint64_t, or other numeric type, this is due to your compiler treating some
       typedef's differently, so it does not correspond to one of the numeric
       types above. I tried using some template hackery to get around this, but
       it did not work. I resorted to just using a uint64_t instead. */
    template<typename T> void serialize(T &value, ISerializer &serializer)
    {
        value.serialize(serializer);
    }

#define KV_MEMBER(member) s(member, #member);

} // namespace CryptoNote
