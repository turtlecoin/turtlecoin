// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <CryptoNote.h>
#include "BinaryInputStreamSerializer.h"
#include "BinaryOutputStreamSerializer.h"
#include "Common/MemoryInputStream.h"
#include "Common/StdInputStream.h"
#include "Common/StdOutputStream.h"
#include "Common/VectorOutputStream.h"

#include <fstream>

namespace CryptoNote
{

    template<typename T>
    BinaryArray storeToBinary(const T &obj)
    {
        BinaryArray result;
        Common::VectorOutputStream stream(result);
        BinaryOutputStreamSerializer ba(stream);
        serialize(const_cast<T &>(obj), ba);
        return result;
    }

    template<typename T>
    void loadFromBinary(T &obj, const BinaryArray &blob)
    {
        Common::MemoryInputStream stream(blob.data(), blob.size());
        BinaryInputStreamSerializer ba(stream);
        serialize(obj, ba);
    }

    template<typename T>
    bool storeToBinaryFile(const T &obj, const std::string &filename)
    {
        try
        {
            std::ofstream dataFile;
            dataFile.open(filename, std::ios_base::binary | std::ios_base::out | std::ios::trunc);
            if (dataFile.fail())
            {
                return false;
            }

            Common::StdOutputStream stream(dataFile);
            BinaryOutputStreamSerializer out(stream);
            CryptoNote::serialize(const_cast<T &>(obj), out);

            if (dataFile.fail())
            {
                return false;
            }

            dataFile.flush();
        } catch (std::exception &)
        {
            return false;
        }

        return true;
    }

    template<class T>
    bool loadFromBinaryFile(T &obj, const std::string &filename)
    {
        try
        {
            std::ifstream dataFile;
            dataFile.open(filename, std::ios_base::binary | std::ios_base::in);
            if (dataFile.fail())
            {
                return false;
            }

            Common::StdInputStream stream(dataFile);
            BinaryInputStreamSerializer in(stream);
            serialize(obj, in);
            return !dataFile.fail();
        } catch (std::exception &)
        {
            return false;
        }
    }

}
