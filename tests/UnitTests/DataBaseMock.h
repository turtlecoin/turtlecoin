// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <unordered_map>
#include <map>

#include <IDataBase.h>
#include "CryptoNoteCore/BlockchainReadBatch.h"
#include "CryptoNoteCore/BlockchainWriteBatch.h"

namespace CryptoNote
{

    class DataBaseMock : public IDataBase
    {
    public:
        DataBaseMock() = default;

        ~DataBaseMock() override;

        std::error_code write(IWriteBatch &batch) override;

        std::error_code writeSync(IWriteBatch &batch) override;

        std::error_code read(IReadBatch &batch) override;

        std::unordered_map<uint32_t, RawBlock> blocks();

        std::map<std::string, std::string> baseState;
    };
}
