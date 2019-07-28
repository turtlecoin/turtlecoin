// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "MemoryBlockchainCacheFactory.h"

namespace CryptoNote
{
    MemoryBlockchainCacheFactory::MemoryBlockchainCacheFactory(
        const std::string &filename,
        std::shared_ptr<Logging::ILogger> logger):
        filename(filename),
        logger(logger)
    {
    }

    MemoryBlockchainCacheFactory::~MemoryBlockchainCacheFactory() {}

    std::unique_ptr<IBlockchainCache> MemoryBlockchainCacheFactory::createRootBlockchainCache(const Currency &currency)
    {
        return createBlockchainCache(currency, nullptr, 0);
    }

    std::unique_ptr<IBlockchainCache> MemoryBlockchainCacheFactory::createBlockchainCache(
        const Currency &currency,
        IBlockchainCache *parent,
        uint32_t startIndex)
    {
        return std::unique_ptr<IBlockchainCache>(new BlockchainCache(filename, currency, logger, parent, startIndex));
    }

} // namespace CryptoNote
