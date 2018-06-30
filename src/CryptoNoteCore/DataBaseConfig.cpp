// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "DataBaseConfig.h"

#include <boost/utility/value_init.hpp>

#include <Common/Util.h>
#include "Common/CommandLine.h"
#include "Common/StringTools.h"
#include "crypto/crypto.h"
#include "CryptoNoteConfig.h"

using namespace CryptoNote;

namespace
{

    const uint64_t WRITE_BUFFER_MB_DEFAULT_SIZE = 256;
    const uint64_t READ_BUFFER_MB_DEFAULT_SIZE = 10;
    const uint32_t DEFAULT_MAX_OPEN_FILES = 100;
    const uint16_t DEFAULT_BACKGROUND_THREADS_COUNT = 2;

    const uint64_t MEGABYTE = 1024 * 1024;

    const command_line::arg_descriptor<uint16_t> argBackgroundThreadsCount = {"db-threads",
                                                                              "Nuber of background threads used for compaction and flush",
                                                                              DEFAULT_BACKGROUND_THREADS_COUNT};
    const command_line::arg_descriptor<uint32_t> argMaxOpenFiles = {"db-max-open-files",
                                                                    "Number of open files that can be used by the DB",
                                                                    DEFAULT_MAX_OPEN_FILES};
    const command_line::arg_descriptor<uint64_t> argWriteBufferSize = {"db-write-buffer-size",
                                                                       "Size of data base write buffer in megabytes",
                                                                       WRITE_BUFFER_MB_DEFAULT_SIZE};
    const command_line::arg_descriptor<uint64_t> argReadCacheSize = {"db-read-cache-size",
                                                                     "Size of data base read cache in megabytes",
                                                                     READ_BUFFER_MB_DEFAULT_SIZE};

} //namespace

void DataBaseConfig::initOptions(boost::program_options::options_description &desc)
{
    command_line::add_arg(desc, argBackgroundThreadsCount);
    command_line::add_arg(desc, argMaxOpenFiles);
    command_line::add_arg(desc, argWriteBufferSize);
    command_line::add_arg(desc, argReadCacheSize);
}

DataBaseConfig::DataBaseConfig() :
        dataDir(Tools::getDefaultDataDirectory()),
        backgroundThreadsCount(DEFAULT_BACKGROUND_THREADS_COUNT),
        maxOpenFiles(DEFAULT_MAX_OPEN_FILES),
        writeBufferSize(WRITE_BUFFER_MB_DEFAULT_SIZE * MEGABYTE),
        readCacheSize(READ_BUFFER_MB_DEFAULT_SIZE * MEGABYTE),
        testnet(false)
{
}

bool DataBaseConfig::init(const boost::program_options::variables_map &vm)
{
    if (vm.count(argBackgroundThreadsCount.name) != 0 &&
        (!vm[argBackgroundThreadsCount.name].defaulted() || backgroundThreadsCount == 0))
    {
        backgroundThreadsCount = command_line::get_arg(vm, argBackgroundThreadsCount);
    }

    if (vm.count(argMaxOpenFiles.name) != 0 && (!vm[argMaxOpenFiles.name].defaulted() || maxOpenFiles == 0))
    {
        maxOpenFiles = command_line::get_arg(vm, argMaxOpenFiles);
    }

    if (vm.count(argWriteBufferSize.name) != 0 && (!vm[argWriteBufferSize.name].defaulted() || writeBufferSize == 0))
    {
        writeBufferSize = command_line::get_arg(vm, argWriteBufferSize) * MEGABYTE;
    }

    if (vm.count(argReadCacheSize.name) != 0 && (!vm[argReadCacheSize.name].defaulted() || readCacheSize == 0))
    {
        readCacheSize = command_line::get_arg(vm, argReadCacheSize) * MEGABYTE;
    }

    if (vm.count(command_line::arg_data_dir.name) != 0 &&
        (!vm[command_line::arg_data_dir.name].defaulted() || dataDir == Tools::getDefaultDataDirectory()))
    {
        dataDir = command_line::get_arg(vm, command_line::arg_data_dir);
    }

    configFolderDefaulted = vm[command_line::arg_data_dir.name].defaulted();

    return true;
}

bool DataBaseConfig::isConfigFolderDefaulted() const
{
    return configFolderDefaulted;
}

std::string DataBaseConfig::getDataDir() const
{
    return dataDir;
}

uint16_t DataBaseConfig::getBackgroundThreadsCount() const
{
    return backgroundThreadsCount;
}

uint32_t DataBaseConfig::getMaxOpenFiles() const
{
    return maxOpenFiles;
}

uint64_t DataBaseConfig::getWriteBufferSize() const
{
    return writeBufferSize;
}

uint64_t DataBaseConfig::getReadCacheSize() const
{
    return readCacheSize;
}

bool DataBaseConfig::getTestnet() const
{
    return testnet;
}

void DataBaseConfig::setConfigFolderDefaulted(bool defaulted)
{
    configFolderDefaulted = defaulted;
}

void DataBaseConfig::setDataDir(const std::string &dataDir)
{
    this->dataDir = dataDir;
}

void DataBaseConfig::setBackgroundThreadsCount(uint16_t backgroundThreadsCount)
{
    this->backgroundThreadsCount = backgroundThreadsCount;
}

void DataBaseConfig::setMaxOpenFiles(uint32_t maxOpenFiles)
{
    this->maxOpenFiles = maxOpenFiles;
}

void DataBaseConfig::setWriteBufferSize(uint64_t writeBufferSize)
{
    this->writeBufferSize = writeBufferSize;
}

void DataBaseConfig::setReadCacheSize(uint64_t readCacheSize)
{
    this->readCacheSize = readCacheSize;
}

void DataBaseConfig::setTestnet(bool testnet)
{
    this->testnet = testnet;
}
