// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <common/MemoryInputStream.h>
#include <common/StringTools.h>
#include <common/VectorOutputStream.h>
#include <config/Constants.h>
#include <config/CryptoNoteConfig.h>
#include <limits>
#include <serialization/BinaryInputStreamSerializer.h>
#include <serialization/BinaryOutputStreamSerializer.h>
#include <serialization/CryptoNoteSerialization.h>
#include <serialization/SerializationTools.h>

namespace CryptoNote
{
    void getBinaryArrayHash(const BinaryArray &binaryArray, Crypto::Hash &hash);

    Crypto::Hash getBinaryArrayHash(const BinaryArray &binaryArray);

    template<class T> bool getObjectBinarySize(const T &object, size_t &size)
    {
        BinaryArray ba;
        if (!toBinaryArray(object, ba))
        {
            size = (std::numeric_limits<size_t>::max)();
            return false;
        }

        size = ba.size();
        return true;
    }

    template<class T> size_t getObjectBinarySize(const T &object)
    {
        size_t size;
        getObjectBinarySize(object, size);
        return size;
    }

    template<class T> bool getObjectHash(const T &object, Crypto::Hash &hash)
    {
        BinaryArray ba;
        if (!toBinaryArray(object, ba))
        {
            hash = Constants::NULL_HASH;
            return false;
        }

        hash = getBinaryArrayHash(ba);
        return true;
    }

    template<class T> bool getObjectHash(const T &object, Crypto::Hash &hash, size_t &size)
    {
        BinaryArray ba;
        if (!toBinaryArray(object, ba))
        {
            hash = Constants::NULL_HASH;
            size = (std::numeric_limits<size_t>::max)();
            return false;
        }

        size = ba.size();
        hash = getBinaryArrayHash(ba);
        return true;
    }

    template<class T> Crypto::Hash getObjectHash(const T &object)
    {
        Crypto::Hash hash;
        getObjectHash(object, hash);
        return hash;
    }

    inline bool getBaseTransactionHash(const BaseTransaction &tx, Crypto::Hash &hash)
    {
        if (tx.version < TRANSACTION_VERSION_2)
        {
            return getObjectHash(tx, hash);
        }
        else
        {
            BinaryArray data {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0xbc, 0x36, 0x78, 0x9e, 0x7a, 0x1e, 0x28, 0x14, 0x36, 0x46,
                               0x42, 0x29, 0x82, 0x8f, 0x81, 0x7d, 0x66, 0x12, 0xf7, 0xb4, 0x77, 0xd6, 0x65, 0x91,
                               0xff, 0x96, 0xa9, 0xe0, 0x64, 0xbc, 0xc9, 0x8a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
            if (getObjectHash(
                    static_cast<const TransactionPrefix &>(tx), *reinterpret_cast<Crypto::Hash *>(data.data())))
            {
                hash = getBinaryArrayHash(data);
                return true;
            }
            else
            {
                return false;
            }
        }
    }

} // namespace CryptoNote
