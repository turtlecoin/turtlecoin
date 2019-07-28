// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "BlockchainCache.h"
#include "CryptoNote.h"
#include "DatabaseCacheData.h"
#include "IWriteBatch.h"

namespace CryptoNote
{
    class BlockchainWriteBatch : public IWriteBatch
    {
      public:
        BlockchainWriteBatch();

        ~BlockchainWriteBatch();

        BlockchainWriteBatch &
            insertSpentKeyImages(uint32_t blockIndex, const std::unordered_set<Crypto::KeyImage> &spentKeyImages);

        BlockchainWriteBatch &
            insertCachedTransaction(const ExtendedTransactionInfo &transaction, uint64_t totalTxsCount);

        BlockchainWriteBatch &insertPaymentId(
            const Crypto::Hash &transactionHash,
            const Crypto::Hash paymentId,
            uint32_t totalTxsCountForPaymentId);

        BlockchainWriteBatch &insertCachedBlock(
            const CachedBlockInfo &block,
            uint32_t blockIndex,
            const std::vector<Crypto::Hash> &blockTxs);

        BlockchainWriteBatch &insertKeyOutputGlobalIndexes(
            IBlockchainCache::Amount amount,
            const std::vector<PackedOutIndex> &outputs,
            uint32_t totalOutputsCountForAmount);

        BlockchainWriteBatch &insertRawBlock(uint32_t blockIndex, const RawBlock &block);

        BlockchainWriteBatch &insertClosestTimestampBlockIndex(uint64_t timestamp, uint32_t blockIndex);

        BlockchainWriteBatch &insertKeyOutputAmounts(
            const std::set<IBlockchainCache::Amount> &amounts,
            uint32_t totalKeyOutputAmountsCount);

        BlockchainWriteBatch &insertTimestamp(uint64_t timestamp, const std::vector<Crypto::Hash> &blockHashes);

        BlockchainWriteBatch &insertKeyOutputInfo(
            IBlockchainCache::Amount amount,
            IBlockchainCache::GlobalOutputIndex globalIndex,
            const KeyOutputInfo &outputInfo);

        BlockchainWriteBatch &
            removeSpentKeyImages(uint32_t blockIndex, const std::vector<Crypto::KeyImage> &spentKeyImages);

        BlockchainWriteBatch &removeCachedTransaction(const Crypto::Hash &transactionHash, uint64_t totalTxsCount);

        BlockchainWriteBatch &removePaymentId(const Crypto::Hash paymentId, uint32_t totalTxsCountForPaytmentId);

        BlockchainWriteBatch &removeCachedBlock(const Crypto::Hash &blockHash, uint32_t blockIndex);

        BlockchainWriteBatch &removeKeyOutputGlobalIndexes(
            IBlockchainCache::Amount amount,
            uint32_t outputsToRemoveCount,
            uint32_t totalOutputsCountForAmount);

        BlockchainWriteBatch &removeRawBlock(uint32_t blockIndex);

        BlockchainWriteBatch &removeClosestTimestampBlockIndex(uint64_t timestamp);

        BlockchainWriteBatch &removeTimestamp(uint64_t timestamp);

        BlockchainWriteBatch &
            removeKeyOutputInfo(IBlockchainCache::Amount amount, IBlockchainCache::GlobalOutputIndex globalIndex);

        std::vector<std::pair<std::string, std::string>> extractRawDataToInsert() override;

        std::vector<std::string> extractRawKeysToRemove() override;

      private:
        std::vector<std::pair<std::string, std::string>> rawDataToInsert;

        std::vector<std::string> rawKeysToRemove;
    };

} // namespace CryptoNote
