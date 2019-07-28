// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "BlockchainCache.h"
#include "IBlockchainCacheFactory.h"

namespace CryptoNote
{
    class MemoryBlockchainCacheFactory : public IBlockchainCacheFactory
    {
      public:
        MemoryBlockchainCacheFactory(const std::string &filename, std::shared_ptr<Logging::ILogger> logger);

        virtual ~MemoryBlockchainCacheFactory() override;

        std::unique_ptr<IBlockchainCache> createRootBlockchainCache(const Currency &currency) override;

        std::unique_ptr<IBlockchainCache>
            createBlockchainCache(const Currency &currency, IBlockchainCache *parent, uint32_t startIndex = 0) override;

      private:
        std::string filename;

        std::shared_ptr<Logging::ILogger> logger;
    };

} // namespace CryptoNote
