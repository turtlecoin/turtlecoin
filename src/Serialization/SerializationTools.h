// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <list>
#include <vector>
#include <Common/MemoryInputStream.h>
#include <Common/StringOutputStream.h>
#include "JsonInputStreamSerializer.h"
#include "JsonOutputStreamSerializer.h"
#include "KVBinaryInputStreamSerializer.h"
#include "KVBinaryOutputStreamSerializer.h"

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
    { return js.getString(); }

    template<>
    inline uint64_t getValueAs<uint64_t>(const JsonValue &js)
    { return static_cast<uint64_t>(js.getInteger()); }

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

    template<typename T>
    Common::JsonValue storeToJsonValue(const std::vector<T> &v)
    { return storeContainerToJsonValue(v); }

    template<typename T>
    Common::JsonValue storeToJsonValue(const std::list<T> &v)
    { return storeContainerToJsonValue(v); }

    template<>
    inline Common::JsonValue storeToJsonValue(const std::string &v)
    { return Common::JsonValue(v); }

    template<typename T>
    void loadFromJsonValue(T &v, const Common::JsonValue &js)
    {
        JsonInputValueSerializer s(js);
        serialize(v, s);
    }

    template<typename T>
    void loadFromJsonValue(std::vector<T> &v, const Common::JsonValue &js)
    {
        for (size_t i = 0; i < js.size(); ++i)
        {
            v.push_back(Common::getValueAs<T>(js[i]));
        }
    }

    template<typename T>
    void loadFromJsonValue(std::list<T> &v, const Common::JsonValue &js)
    {
        for (size_t i = 0; i < js.size(); ++i)
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
    bool loadFromJson(T &v, const std::string &buf)
    {
        try
        {
            if (buf.empty())
            {
                return true;
            }
            auto js = Common::JsonValue::fromString(buf);
            loadFromJsonValue(v, js);
        } catch (std::exception &)
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
    bool loadFromBinaryKeyValue(T &v, const std::string &buf)
    {
        try
        {
            Common::MemoryInputStream stream(buf.data(), buf.size());
            KVBinaryInputStreamSerializer s(stream);
            serialize(v, s);
            return true;
        } catch (std::exception &)
        {
            return false;
        }
    }

}
