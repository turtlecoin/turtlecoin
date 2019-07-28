// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "BinaryInputStreamSerializer.h"

#include "SerializationOverloads.h"

#include <algorithm>
#include <cassert>
#include <common/StreamTools.h>
#include <config/CryptoNoteConfig.h>
#include <stdexcept>

using namespace Common;

namespace CryptoNote
{
    namespace
    {
        template<typename StorageType, typename T> void readVarintAs(IInputStream &s, T &i)
        {
            i = static_cast<T>(readVarint<StorageType>(s));
        }

    } // namespace

    ISerializer::SerializerType BinaryInputStreamSerializer::type() const
    {
        return ISerializer::INPUT;
    }

    bool BinaryInputStreamSerializer::beginObject(Common::StringView name)
    {
        return true;
    }

    void BinaryInputStreamSerializer::endObject() {}

    bool BinaryInputStreamSerializer::beginArray(uint64_t &size, Common::StringView name)
    {
        readVarintAs<uint64_t>(stream, size);
        return true;
    }

    void BinaryInputStreamSerializer::endArray() {}

    bool BinaryInputStreamSerializer::operator()(uint8_t &value, Common::StringView name)
    {
        readVarint(stream, value);
        return true;
    }

    bool BinaryInputStreamSerializer::operator()(uint16_t &value, Common::StringView name)
    {
        readVarint(stream, value);
        return true;
    }

    bool BinaryInputStreamSerializer::operator()(int16_t &value, Common::StringView name)
    {
        readVarintAs<uint16_t>(stream, value);
        return true;
    }

    bool BinaryInputStreamSerializer::operator()(uint32_t &value, Common::StringView name)
    {
        readVarint(stream, value);
        return true;
    }

    bool BinaryInputStreamSerializer::operator()(int32_t &value, Common::StringView name)
    {
        readVarintAs<uint32_t>(stream, value);
        return true;
    }

    bool BinaryInputStreamSerializer::operator()(int64_t &value, Common::StringView name)
    {
        readVarintAs<uint64_t>(stream, value);
        return true;
    }

    bool BinaryInputStreamSerializer::operator()(uint64_t &value, Common::StringView name)
    {
        readVarint(stream, value);
        return true;
    }

    bool BinaryInputStreamSerializer::operator()(bool &value, Common::StringView name)
    {
        value = read<uint8_t>(stream) != 0;
        return true;
    }

    bool BinaryInputStreamSerializer::operator()(std::string &value, Common::StringView name)
    {
        uint64_t size;
        readVarint(stream, size);

        /* Can't take up more than a block size */
        if (size > CryptoNote::parameters::MAX_EXTRA_SIZE && std::string(name.getData()) == "mm_tag")
        {
            std::vector<char> temp;
            temp.resize(1);

            /* Read to the end of the stream, and throw the data away, otherwise
               transaction won't validate. There should be a better way to do this? */
            while (size > 0)
            {
                checkedRead(&temp[0], 1);
                size--;
            }

            value.clear();
            return true;
        }

        if (size > 0)
        {
            std::vector<char> temp;
            temp.resize(size);
            checkedRead(&temp[0], size);
            value.reserve(size);
            value.assign(&temp[0], size);
        }
        else
        {
            value.clear();
        }

        return true;
    }

    bool BinaryInputStreamSerializer::binary(void *value, uint64_t size, Common::StringView name)
    {
        checkedRead(static_cast<char *>(value), size);
        return true;
    }

    bool BinaryInputStreamSerializer::binary(std::string &value, Common::StringView name)
    {
        return (*this)(value, name);
    }

    bool BinaryInputStreamSerializer::operator()(double &value, Common::StringView name)
    {
        assert(false); // the method is not supported for this type of serialization
        throw std::runtime_error("double serialization is not supported in BinaryInputStreamSerializer");
        return false;
    }

    void BinaryInputStreamSerializer::checkedRead(char *buf, uint64_t size)
    {
        read(stream, buf, size);
    }

} // namespace CryptoNote
