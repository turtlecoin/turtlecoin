// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IBlockchainCacheFactory.h"
#include <Logging/LoggerMessage.h>

namespace CryptoNote
{

    class IDataBase;

    class DatabaseBlockchainCacheFactory : public IBlockchainCacheFactory
    {
    public:
        explicit DatabaseBlockchainCacheFactory(IDataBase &database, Logging::ILogger &logger);

        virtual ~DatabaseBlockchainCacheFactory();

        virtual std::unique_ptr<IBlockchainCache> createRootBlockchainCache(const Currency &currency) override;

        virtual std::unique_ptr<IBlockchainCache>
        createBlockchainCache(const Currency &currency, IBlockchainCache *parent, uint32_t startIndex = 0) override;

    private:
        IDataBase &database;
        Logging::ILogger &logger;
    };

} //namespace CryptoNote
