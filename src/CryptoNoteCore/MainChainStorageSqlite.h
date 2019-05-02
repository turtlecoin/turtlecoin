// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IMainChainStorage.h"

#include "Currency.h"

#include "sqlite3.h"

namespace CryptoNote
{
    class MainChainStorageSqlite : public IMainChainStorage
    {
        public:
            MainChainStorageSqlite(const std::string &blocksFilename, const std::string &indexesFilename);

            virtual ~MainChainStorageSqlite();

            virtual void pushBlock(const RawBlock &rawBlock) override;
            virtual void popBlock() override;
            void rewindTo(const uint32_t index) const override;

            virtual RawBlock getBlockByIndex(uint32_t index) const override;
            virtual uint32_t getBlockCount() const override;

            virtual void clear() override;

        private:
            sqlite3 *m_db;
    };

    std::unique_ptr<IMainChainStorage> createSwappedMainChainStorageSqlite(const std::string &dataDir, const Currency &currency);
}