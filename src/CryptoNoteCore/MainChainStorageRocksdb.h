// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IMainChainStorage.h"

#include "Currency.h"

#include "rocksdb/db.h"

#include "DataBaseConfig.h"

#include <memory>

namespace CryptoNote
{
    class MainChainStorageRocksdb : public IMainChainStorage
    {
        public:
            MainChainStorageRocksdb(
              const std::string &blocksFilename,
              const std::string &indexesFilename,
              const DataBaseConfig& config
            );

            virtual ~MainChainStorageRocksdb();

            virtual void pushBlock(const RawBlock &rawBlock) override;
            virtual void popBlock() override;
            virtual void rewindTo(const uint32_t index) const override;

            virtual RawBlock getBlockByIndex(const uint32_t index) const override;
            virtual uint32_t getBlockCount() const override;

            virtual void clear() override;
            

        private:
            void initializeBlockCount();
            std::unique_ptr<rocksdb::DB> m_db;
            mutable std::atomic_uint m_blockcount;
    };

    std::unique_ptr<IMainChainStorage> createSwappedMainChainStorageRocksdb(
      const std::string &dataDir,
      const Currency &currency,
      const DataBaseConfig& config
    );
}