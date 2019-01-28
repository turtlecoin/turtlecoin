// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include "rapidjson/document.h"

/* Yikes! */
typedef rapidjson::GenericObject<true, rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>>> JSONObject;

typedef rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>> JSONValue;

static const std::string kTypeNames[] =
{ 
    "Null", "False", "True", "Object", "Array", "String", "Number"
};

template<typename T>
const rapidjson::Value & getJsonValue(const T &j, const std::string &key)
{
    auto val = j.FindMember(key);

    if (val == j.MemberEnd())
    {
        throw std::invalid_argument("Missing JSON parameter: '" + key + "'");
    }

    return val->value;
}

template<typename T>
uint64_t getUint64FromJSON(const T &j, const std::string &key)
{
    auto &val = getJsonValue(j, key);

    if (!val.IsUint64())
    {
        throw std::invalid_argument(
            "JSON parameter is wrong type. Expected uint64_t, got " +
            kTypeNames[val.GetType()]
        );
    }

    return val.GetUint64();
}

template<typename T>
uint64_t getInt64FromJSON(const T &j, const std::string &key)
{
    auto &val = getJsonValue(j, key);

    if (!val.IsInt64())
    {
        throw std::invalid_argument(
            "JSON parameter is wrong type. Expected int64_t, got " +
            kTypeNames[val.GetType()]
        );
    }

    return val.GetInt64();
}

/**
 * Gets a string from the JSON, with a given keyname
 */
template<typename T>
std::string getStringFromJSON(const T &j, const std::string &key)
{
    auto &val = getJsonValue(j, key);

    if (!val.IsString())
    {
        throw std::invalid_argument(
            "JSON parameter is wrong type. Expected String, got " +
            kTypeNames[val.GetType()]
        );
    }

    return val.GetString();
}

/**
 * Gets a string from the JSON, without a key. For example, we might have an
 * array of strings.
 */
template<typename T>
std::string getStringFromJSONString(const T &j)
{
    if (!j.IsString())
    {
        throw std::invalid_argument(
            "JSON parameter is wrong type. Expected String, got " +
            kTypeNames[j.GetType()]
        );
    }

    return j.GetString();
}

template<typename T>
auto getArrayFromJSON(const T &j, const std::string &key)
{
    auto &val = getJsonValue(j, key);

    if (!val.IsArray())
    {
        throw std::invalid_argument(
            "JSON parameter is wrong type. Expected Array, got " +
            kTypeNames[val.GetType()]
        );
    }

    return val.GetArray();
}

template<typename T>
JSONObject getObjectFromJSON(const T &j, const std::string &key)
{
    auto &val = getJsonValue(j, key);

    if (!val.IsObject())
    {
        throw std::invalid_argument(
            "JSON parameter is wrong type. Expected Object, got " +
            kTypeNames[val.GetType()]
        );
    }

    return val.Get_Object();
}

template<typename T>
bool getBoolFromJSON(const T &j, const std::string &key)
{
    auto &val = getJsonValue(j, key);

    if (!val.IsBool())
    {
        throw std::invalid_argument(
            "JSON parameter is wrong type. Expected Bool, got " +
            kTypeNames[val.GetType()]
        );
    }

    return val.GetBool();
}
