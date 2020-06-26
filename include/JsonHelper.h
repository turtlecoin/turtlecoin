// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "rapidjson/document.h"

/* Yikes! */
typedef rapidjson::GenericObject<
    true,
    rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>>>
    JSONObject;

typedef rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>>
    JSONValue;

static const std::string kTypeNames[] = {"Null", "False", "True", "Object", "Array", "String", "Number", "Double"};

template<typename T> bool hasMember(const T &j, const std::string &key)
{
    auto val = j.FindMember(key);

    return val != j.MemberEnd();
}

/**
 * Gets a JSONValue from the JSON with a given keyname
 */
template<typename T> const rapidjson::Value &getJsonValue(const T &j, const std::string &key)
{
    auto val = j.FindMember(key);

    if (val == j.MemberEnd())
    {
        throw std::invalid_argument("Missing JSON parameter: '" + key + "'");
    }

    return val->value;
}

/**
 * Gets a uint32_t from the JSON, with or without a given keyname
 */
template<typename T> uint32_t getUintFromJSON(const T &j)
{
    if (!j.IsUint())
    {
        throw std::invalid_argument(
            "JSON parameter is wrong type. Expected uint64_t, got " + kTypeNames[j.GetType()]);
    }

    return j.GetUint();
}


template<typename T> uint32_t getUintFromJSON(const T &j, const std::string &key)
{
    auto &val = getJsonValue(j, key);

    return getUintFromJSON(val);
}

/**
 * Gets a double from the JSON, with or without a given keyname
 */
template<typename T> double getDoubleFromJSON(const T &j)
{
    if (!j.IsDouble())
    {
        throw std::invalid_argument(
            "JSON parameter is wrong type. Expected double, got " + kTypeNames[j.GetType()]);
    }

    return j.GetDouble();
}

template<typename T> double getDoubleFromJSON(const T &j, const std::string &key)
{
    auto &val = getJsonValue(j, key);

    return getDoubleFromJSON(val);
}

/**
 * Gets a uint64_t from the JSON, with or without a given keyname
 */
template<typename T> uint64_t getUint64FromJSON(const T &j)
{
    if (!j.IsUint64())
    {
        throw std::invalid_argument(
            "JSON parameter is wrong type. Expected uint64_t, got " + kTypeNames[j.GetType()]);
    }

    return j.GetUint64();
}

template<typename T> uint64_t getUint64FromJSON(const T &j, const std::string &key)
{
    auto &val = getJsonValue(j, key);

    return getUint64FromJSON(val);
}

/**
 * Gets a int64_t from the JSON, with or without a given keyname
 */
template<typename T> uint64_t getInt64FromJSON(const T &j, const std::string &key)
{
    auto &val = getJsonValue(j, key);

    if (!val.IsInt64())
    {
        throw std::invalid_argument("JSON parameter is wrong type. Expected int64_t, got " + kTypeNames[val.GetType()]);
    }

    return val.GetInt64();
}

/**
 * Gets a string from the JSON, with or without a given keyname
 */
template<typename T> std::string getStringFromJSON(const T &j)
{
    if (!j.IsString())
    {
        throw std::invalid_argument("JSON parameter is wrong type. Expected String, got " + kTypeNames[j.GetType()]);
    }

    return j.GetString();
}

template<typename T> std::string getStringFromJSON(const T &j, const std::string &key)
{
    auto &val = getJsonValue(j, key);

    return getStringFromJSON(val);
}

/**
 * Gets an Array from JSON, with or without a given keyname
 */
template<typename T> auto getArrayFromJSON(const T &j)
{
    if (!j.IsArray())
    {
        throw std::invalid_argument("JSON parameter is wrong type. Expected Array, got " + kTypeNames[j.GetType()]);
    }

    return j.GetArray();
}

template<typename T> auto getArrayFromJSON(const T &j, const std::string &key)
{
    auto &val = getJsonValue(j, key);

    return getArrayFromJSON(val);

    return val.GetArray();
}

/**
 * Gets a JSONObject from JSON, with our without a given keyname
 */
template<typename T> JSONObject getObjectFromJSON(const T &j)
{
    if (!j.IsObject())
    {
        throw std::invalid_argument("JSON parameter is wrong type. Expected Object, got " + kTypeNames[j.GetType()]);
    }

    return j.Get_Object();
}

template<typename T> JSONObject getObjectFromJSON(const T &j, const std::string &key)
{
    auto &val = getJsonValue(j, key);

    return getObjectFromJSON(val);
}

/**
 * Gets a boolean from JSON, with our without a given keyname
 */
template<typename T> bool getBoolFromJSON(const T &j)
{
    if (!j.IsBool())
    {
        throw std::invalid_argument("JSON parameter is wrong type. Expected Bool, got " + kTypeNames[j.GetType()]);
    }

    return j.GetBool();
}

template<typename T> bool getBoolFromJSON(const T &j, const std::string &key)
{
    auto &val = getJsonValue(j, key);

    return getBoolFromJSON(val);
}
