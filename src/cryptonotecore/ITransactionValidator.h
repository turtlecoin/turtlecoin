// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "cryptonotecore/CryptoNoteBasic.h"

namespace CryptoNote
{
    struct BlockInfo
    {
        uint32_t height;

        Crypto::Hash id;

        BlockInfo()
        {
            clear();
        }

        void clear()
        {
            height = 0;
            id = Constants::NULL_HASH;
        }

        bool empty() const
        {
            return id == Constants::NULL_HASH;
        }
    };

    class ITransactionValidator
    {
      public:
        virtual ~ITransactionValidator() {}

        virtual bool checkTransactionInputs(const CryptoNote::Transaction &tx, BlockInfo &maxUsedBlock) = 0;

        virtual bool checkTransactionInputs(
            const CryptoNote::Transaction &tx,
            BlockInfo &maxUsedBlock,
            BlockInfo &lastFailed) = 0;

        virtual bool haveSpentKeyImages(const CryptoNote::Transaction &tx) = 0;

        virtual bool checkTransactionSize(size_t blobSize) = 0;
    };

} // namespace CryptoNote
