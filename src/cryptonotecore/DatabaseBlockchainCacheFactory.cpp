// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "DatabaseBlockchainCacheFactory.h"

#include "BlockchainCache.h"
#include "DatabaseBlockchainCache.h"
#include "IDataBase.h"

namespace CryptoNote
{
    DatabaseBlockchainCacheFactory::DatabaseBlockchainCacheFactory(
        IDataBase &database,
        std::shared_ptr<Logging::ILogger> logger):
        database(database),
        logger(logger)
    {
    }

    DatabaseBlockchainCacheFactory::~DatabaseBlockchainCacheFactory() {}

    std::unique_ptr<IBlockchainCache>
        DatabaseBlockchainCacheFactory::createRootBlockchainCache(const Currency &currency)
    {
        return std::unique_ptr<IBlockchainCache>(new DatabaseBlockchainCache(currency, database, *this, logger));
    }

    std::unique_ptr<IBlockchainCache> DatabaseBlockchainCacheFactory::createBlockchainCache(
        const Currency &currency,
        IBlockchainCache *parent,
        uint32_t startIndex)
    {
        return std::unique_ptr<IBlockchainCache>(new BlockchainCache("", currency, logger, parent, startIndex));
    }

} // namespace CryptoNote
