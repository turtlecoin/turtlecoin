// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace CryptoNote
{

class DataBaseConfig
{
  public:
    DataBaseConfig();
    bool init(const std::string dataDirectory, const int backgroundThreads, const int maxOpenFiles,
              const int writeBufferSizeMB, const int readCacheSizeMB);

    bool isConfigFolderDefaulted() const;
    std::string getDataDir() const;
    uint16_t getBackgroundThreadsCount() const;
    uint32_t getMaxOpenFiles() const;
    uint64_t getWriteBufferSize() const; // Bytes
    uint64_t getReadCacheSize() const;   // Bytes
    bool getTestnet() const;

  private:
    bool configFolderDefaulted;
    std::string dataDir;
    uint16_t backgroundThreadsCount;
    uint32_t maxOpenFiles;
    uint64_t writeBufferSize;
    uint64_t readCacheSize;
    bool testnet;
};
} // namespace CryptoNote
