// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "MainChainStorage.h"

#include <boost/filesystem.hpp>

#include "CryptoNoteTools.h"

namespace CryptoNote
{

    const size_t STORAGE_CACHE_SIZE = 100;

    MainChainStorage::MainChainStorage(const std::string &blocksFilename, const std::string &indexesFilename)
    {
        if (!storage.open(blocksFilename, indexesFilename, STORAGE_CACHE_SIZE))
        {
            throw std::runtime_error("Failed to load main chain storage: " + blocksFilename);
        }
    }

    MainChainStorage::~MainChainStorage()
    {
        storage.close();
    }

    void MainChainStorage::pushBlock(const RawBlock &rawBlock)
    {
        storage.push_back(rawBlock);
    }

    void MainChainStorage::popBlock()
    {
        storage.pop_back();
    }

    RawBlock MainChainStorage::getBlockByIndex(uint32_t index) const
    {
        if (index >= storage.size())
        {
            throw std::out_of_range("Block index " + std::to_string(index) + " is out of range. Blocks count: " +
                                    std::to_string(storage.size()));
        }

        return storage[index];
    }

    uint32_t MainChainStorage::getBlockCount() const
    {
        return static_cast<uint32_t>(storage.size());
    }

    void MainChainStorage::clear()
    {
        storage.clear();
    }

    std::unique_ptr<IMainChainStorage>
    createSwappedMainChainStorage(const std::string &dataDir, const Currency &currency)
    {
        boost::filesystem::path blocksFilename = boost::filesystem::path(dataDir) / currency.blocksFileName();
        boost::filesystem::path indexesFilename = boost::filesystem::path(dataDir) / currency.blockIndexesFileName();

        std::unique_ptr<IMainChainStorage> storage(
                new MainChainStorage(blocksFilename.string(), indexesFilename.string()));
        if (storage->getBlockCount() == 0)
        {
            RawBlock genesis;
            genesis.block = toBinaryArray(currency.genesisBlock());
            storage->pushBlock(genesis);
        }

        return storage;
    }

}
