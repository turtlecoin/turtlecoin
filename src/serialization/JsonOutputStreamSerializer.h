// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "../common/JsonValue.h"
#include "ISerializer.h"

#include <iostream>

namespace CryptoNote
{
    class JsonOutputStreamSerializer : public ISerializer
    {
      public:
        JsonOutputStreamSerializer();

        virtual ~JsonOutputStreamSerializer();

        SerializerType type() const override;

        virtual bool beginObject(Common::StringView name) override;

        virtual void endObject() override;

        virtual bool beginArray(uint64_t &size, Common::StringView name) override;

        virtual void endArray() override;

        virtual bool operator()(uint8_t &value, Common::StringView name) override;

        virtual bool operator()(int16_t &value, Common::StringView name) override;

        virtual bool operator()(uint16_t &value, Common::StringView name) override;

        virtual bool operator()(int32_t &value, Common::StringView name) override;

        virtual bool operator()(uint32_t &value, Common::StringView name) override;

        virtual bool operator()(int64_t &value, Common::StringView name) override;

        virtual bool operator()(uint64_t &value, Common::StringView name) override;

        virtual bool operator()(double &value, Common::StringView name) override;

        virtual bool operator()(bool &value, Common::StringView name) override;

        virtual bool operator()(std::string &value, Common::StringView name) override;

        virtual bool binary(void *value, uint64_t size, Common::StringView name) override;

        virtual bool binary(std::string &value, Common::StringView name) override;

        template<typename T> bool operator()(T &value, Common::StringView name)
        {
            return ISerializer::operator()(value, name);
        }

        const Common::JsonValue &getValue() const
        {
            return root;
        }

        friend std::ostream &operator<<(std::ostream &out, const JsonOutputStreamSerializer &enumerator);

      private:
        Common::JsonValue root;

        std::vector<Common::JsonValue *> chain;
    };

} // namespace CryptoNote
