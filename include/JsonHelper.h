// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

/* Yikes! */
typedef rapidjson::GenericObject<true, rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>>> JSONObject;

typedef rapidjson::GenericValue<rapidjson::UTF8<char>, rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>> JSONValue;

static const std::string kTypeNames[] =
{ 
    "Null", "False", "True", "Object", "Array", "String", "Number"
};

class JsonException : public std::exception 
{
	std::string message;

    public:
        JsonException(std::string err) 
        {
            message = err;
        }

        const char* what () const throw () 
        {
            return message.c_str();
        }
};

/*  A JSONBody struct, used to simplify the creation of JSON objects
    that will be used as the body of some HTTP POST request */
struct JSONBody 
{ 
    rapidjson::Value params;
    std::string methodName;

    JSONBody() { }
    
    void setParams (rapidjson::Value& val) 
    {
        params = val;
    }

    void setMethodName (std::string s) 
    {
        methodName = s;
    }

    /*  Upon calling getString(), the resulting JSON will be:
        {
            "jsonrpc": "2.0",
            "method": methodName,
            "params": params
        } 

        If no methodName was given, it will result in a "" string value.
        If no params was given, it will result in a null value 
        (this is expected behaviour). */
    std::string toJSONString() const 
    {
        rapidjson::Document d;
        rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

        d.SetObject();
        d.AddMember("jsonrpc", "2.0", d.GetAllocator());
        d.AddMember("method", methodName, d.GetAllocator());
        d.AddMember("params", rapidjson::Value(params, d.GetAllocator()), d.GetAllocator());
        
        d.Accept(writer);
        return buffer.GetString();
    }
};

template<typename T>
const rapidjson::Value & getJsonValue(const T &j, const std::string &key)
{
    auto val = j.FindMember(key);

    if (val == j.MemberEnd())
    {
        throw JsonException("Missing JSON parameter: '" + key + "'");
    }

    return val->value;
}

template<typename T>
uint64_t getUint64FromJSON(const T &j, const std::string &key)
{
    auto &val = getJsonValue(j, key);

    if (!val.IsUint64())
    {
        throw JsonException(
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
        throw JsonException(
            "JSON parameter is wrong type. Expected int64_t, got " +
            kTypeNames[val.GetType()]
        );
    }

    return val.GetInt64();
}

template<typename T>
uint32_t getUintFromJSON(const T &j, const std::string &key)
{
    auto &val = getJsonValue(j, key);

    if (!val.IsUint())
    {
        throw JsonException(
            "JSON parameter is wrong type. Expected unsigned int, got " +
            kTypeNames[val.GetType()]
        );
    }

    return val.GetUint();
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
        throw JsonException(
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
        throw JsonException(
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
        throw JsonException(
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
        throw JsonException(
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
        throw JsonException(
            "JSON parameter is wrong type. Expected Bool, got " +
            kTypeNames[val.GetType()]
        );
    }

    return val.GetBool();
}
