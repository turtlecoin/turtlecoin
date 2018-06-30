// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include <boost/program_options.hpp>

namespace CryptoNote
{

    class DataBaseConfig
    {
    public:
        DataBaseConfig();

        static void initOptions(boost::program_options::options_description &desc);

        bool init(const boost::program_options::variables_map &vm);

        bool isConfigFolderDefaulted() const;

        std::string getDataDir() const;

        uint16_t getBackgroundThreadsCount() const;

        uint32_t getMaxOpenFiles() const;

        uint64_t getWriteBufferSize() const; //Bytes
        uint64_t getReadCacheSize() const; //Bytes
        bool getTestnet() const;

        void setConfigFolderDefaulted(bool defaulted);

        void setDataDir(const std::string &dataDir);

        void setBackgroundThreadsCount(uint16_t backgroundThreadsCount);

        void setMaxOpenFiles(uint32_t maxOpenFiles);

        void setWriteBufferSize(uint64_t writeBufferSize); //Bytes
        void setReadCacheSize(uint64_t readCacheSize); //Bytes
        void setTestnet(bool testnet);

    private:
        bool configFolderDefaulted;
        std::string dataDir;
        uint16_t backgroundThreadsCount;
        uint32_t maxOpenFiles;
        uint64_t writeBufferSize;
        uint64_t readCacheSize;
        bool testnet;
    };
} //namespace CryptoNote
