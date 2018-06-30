// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "rocksdb/db.h"

#include "IDataBase.h"
#include "DataBaseConfig.h"

#include <Logging/LoggerRef.h>

namespace CryptoNote
{

    class RocksDBWrapper : public IDataBase
    {
    public:
        RocksDBWrapper(Logging::ILogger &logger);

        virtual ~RocksDBWrapper();

        RocksDBWrapper(const RocksDBWrapper &) = delete;

        RocksDBWrapper(RocksDBWrapper &&) = delete;

        RocksDBWrapper &operator=(const RocksDBWrapper &) = delete;

        RocksDBWrapper &operator=(RocksDBWrapper &&) = delete;

        void init(const DataBaseConfig &config);

        void shutdown();

        void destroy(const DataBaseConfig &config); //Be careful with this method!

        std::error_code write(IWriteBatch &batch) override;

        std::error_code writeSync(IWriteBatch &batch) override;

        std::error_code read(IReadBatch &batch) override;

    private:
        std::error_code write(IWriteBatch &batch, bool sync);

        rocksdb::Options getDBOptions(const DataBaseConfig &config);

        std::string getDataDir(const DataBaseConfig &config);

        enum State
        {
            NOT_INITIALIZED,
            INITIALIZED
        };

        Logging::LoggerRef logger;
        std::unique_ptr<rocksdb::DB> db;
        std::atomic<State> state;
    };
}
