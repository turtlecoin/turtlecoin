// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <list>
#include <vector>
#include <common/MemoryInputStream.h>
#include <common/StringOutputStream.h>
#include <common/VectorOutputStream.h>
#include "JsonInputStreamSerializer.h"
#include "JsonOutputStreamSerializer.h"
#include "KVBinaryInputStreamSerializer.h"
#include "KVBinaryOutputStreamSerializer.h"
#include <zedwallet/Types.h>

#include <serialization/BinaryInputStreamSerializer.h>
#include <serialization/BinaryOutputStreamSerializer.h>
#include <serialization/CryptoNoteSerialization.h>

namespace Common
{

    template<typename T>
    T getValueAs(const JsonValue &js)
    {
        return js;
        //cdstatic_assert(false, "undefined conversion");
    }

    template<>
    inline std::string getValueAs<std::string>(const JsonValue &js)
    {
        return js.getString();
    }

    template<>
    inline uint64_t getValueAs<uint64_t>(const JsonValue &js)
    {
        return static_cast<uint64_t>(js.getInteger());
    }

}

namespace CryptoNote
{

    template<typename T>
    Common::JsonValue storeToJsonValue(const T &v)
    {
        JsonOutputStreamSerializer s;
        serialize(const_cast<T &>(v), s);
        return s.getValue();
    }

    template<typename T>
    Common::JsonValue storeContainerToJsonValue(const T &cont)
    {
        Common::JsonValue js(Common::JsonValue::ARRAY);
        for (const auto &item : cont)
        {
            js.pushBack(item);
        }
        return js;
    }

    template<>
    inline Common::JsonValue storeContainerToJsonValue(const std::vector<AddressBookEntry> &cont)
    {
        Common::JsonValue js(Common::JsonValue::ARRAY);
        for (const auto &item : cont)
        {
            js.pushBack(storeToJsonValue(item));
        }
        return js;
    }

    template<typename T>
    Common::JsonValue storeToJsonValue(const std::vector<T> &v)
    {
        return storeContainerToJsonValue(v);
    }

    template<typename T>
    Common::JsonValue storeToJsonValue(const std::list<T> &v)
    {
        return storeContainerToJsonValue(v);
    }

    template<>
    inline Common::JsonValue storeToJsonValue(const std::string &v)
    {
        return Common::JsonValue(v);
    }

    template<typename T>
    void loadFromJsonValue(
        T &v,
        const Common::JsonValue &js
    )
    {
        JsonInputValueSerializer s(js);
        serialize(v, s);
    }

    template<typename T>
    void loadFromJsonValue(
        std::vector<T> &v,
        const Common::JsonValue &js
    )
    {
        for (uint64_t i = 0; i < js.size(); ++i)
        {
            v.push_back(Common::getValueAs<T>(js[i]));
        }
    }

    template<>
    inline void loadFromJsonValue(
        AddressBook &v,
        const Common::JsonValue &js
    )
    {
        for (uint64_t i = 0; i < js.size(); ++i)
        {
            AddressBookEntry type;
            loadFromJsonValue(type, js[i]);
            v.push_back(type);
        }
    }

    template<typename T>
    void loadFromJsonValue(
        std::list<T> &v,
        const Common::JsonValue &js
    )
    {
        for (uint64_t i = 0; i < js.size(); ++i)
        {
            v.push_back(Common::getValueAs<T>(js[i]));
        }
    }

    template<typename T>
    std::string storeToJson(const T &v)
    {
        return storeToJsonValue(v).toString();
    }

    template<typename T>
    bool loadFromJson(
        T &v,
        const std::string &buf
    )
    {
        try
        {
            if (buf.empty())
            {
                return true;
            }
            auto js = Common::JsonValue::fromString(buf);
            loadFromJsonValue(v, js);
        }
        catch (std::exception &)
        {
            return false;
        }
        return true;
    }

    template<typename T>
    std::string storeToBinaryKeyValue(const T &v)
    {
        KVBinaryOutputStreamSerializer s;
        serialize(const_cast<T &>(v), s);

        std::string result;
        Common::StringOutputStream stream(result);
        s.dump(stream);
        return result;
    }

    template<typename T>
    bool loadFromBinaryKeyValue(
        T &v,
        const std::string &buf
    )
    {
        try
        {
            Common::MemoryInputStream stream(buf.data(), buf.size());
            KVBinaryInputStreamSerializer s(stream);
            serialize(v, s);
            return true;
        }
        catch (std::exception &)
        {
            return false;
        }
    }

    // throws exception if serialization failed
    template<class T>
    std::vector<uint8_t> toBinaryArray(const T &object)
    {
        std::vector<uint8_t> ba;
        Common::VectorOutputStream stream(ba);
        BinaryOutputStreamSerializer serializer(stream);
        serialize(const_cast<T &>(object), serializer);
        return ba;
    }

    // noexcept
    template<class T>
    bool toBinaryArray(
        const T &object,
        std::vector<uint8_t> &binaryArray
    )
    {
        try
        {
            binaryArray = toBinaryArray(object);
        }
        catch (std::exception &)
        {
            return false;
        }

        return true;
    }

    template<>
    inline bool toBinaryArray(
        const std::vector<uint8_t> &object,
        std::vector<uint8_t> &binaryArray
    )
    {
        try
        {
            Common::VectorOutputStream stream(binaryArray);
            BinaryOutputStreamSerializer serializer(stream);
            std::string oldBlob = Common::asString(object);
            serializer(oldBlob, "");
        }
        catch (std::exception &)
        {
            return false;
        }

        return true;
    }

    template<class T>
    T fromBinaryArray(const std::vector<uint8_t> &binaryArray)
    {
        T object;
        Common::MemoryInputStream stream(binaryArray.data(), binaryArray.size());
        BinaryInputStreamSerializer serializer(stream);
        serialize(object, serializer);
        if (!stream.endOfStream())
        { // check that all data was consumed
            throw std::runtime_error("failed to unpack type");
        }

        return object;
    }

    template<class T>
    bool fromBinaryArray(
        T &object,
        const std::vector<uint8_t> &binaryArray
    )
    {
        try
        {
            object = fromBinaryArray<T>(binaryArray);
        }
        catch (std::exception &)
        {
            return false;
        }

        return true;
    }
}
