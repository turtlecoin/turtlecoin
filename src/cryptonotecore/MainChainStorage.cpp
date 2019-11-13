// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "MainChainStorage.h"

#include "common/CryptoNoteTools.h"
#include "common/FileSystemShim.h"
#include <sstream>

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

    void MainChainStorage::rewindTo(const uint32_t index) const
    {
        while (getBlockCount() >= index)
        {
            storage.pop_back();
        }
    }

    RawBlock MainChainStorage::getBlockByIndex(uint32_t index) const
    {
        if (index >= storage.size())
        {
            throw std::out_of_range(
                "Block index " + std::to_string(index)
                + " is out of range. Blocks count: " + std::to_string(storage.size()));
        }

        try
        {
            return storage[index];
        }
        catch (std::exception &)
        {
            /* Intercept the exception here and display a friendly help message */
            std::stringstream errorMessage;

            errorMessage << "Local blockchain cache corruption detected." << std::endl
                         << "Block with index " << std::to_string(index)
                         << " could not be deserialized from the blockchain cache."
                         << std::endl << std::endl
                         << "Please try to repair this issue by starting the node with the option: "
                         << "--rewind-to-height " << std::to_string(index - 1) << std::endl
                         << "If the above does not repair the issue, "
                         << "please launch the node with the option: --resync" << std::endl;

            throw std::runtime_error(errorMessage.str());
        }
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
        fs::path blocksFilename = fs::path(dataDir) / currency.blocksFileName();
        fs::path indexesFilename = fs::path(dataDir) / currency.blockIndexesFileName();

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

} // namespace CryptoNote
