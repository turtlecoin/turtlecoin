// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "BlockchainWriteBatch.h"

#include "DBUtils.h"

using namespace CryptoNote;

BlockchainWriteBatch::BlockchainWriteBatch() {}

BlockchainWriteBatch::~BlockchainWriteBatch() {}

BlockchainWriteBatch &BlockchainWriteBatch::insertSpentKeyImages(
    uint32_t blockIndex,
    const std::unordered_set<Crypto::KeyImage> &spentKeyImages)
{
    rawDataToInsert.reserve(rawDataToInsert.size() + spentKeyImages.size() + 1);
    rawDataToInsert.emplace_back(DB::serialize(DB::BLOCK_INDEX_TO_KEY_IMAGE_PREFIX, blockIndex, spentKeyImages));
    for (const Crypto::KeyImage &keyImage : spentKeyImages)
    {
        rawDataToInsert.emplace_back(DB::serialize(DB::KEY_IMAGE_TO_BLOCK_INDEX_PREFIX, keyImage, blockIndex));
    }
    return *this;
}

BlockchainWriteBatch &
    BlockchainWriteBatch::insertCachedTransaction(const ExtendedTransactionInfo &transaction, uint64_t totalTxsCount)
{
    rawDataToInsert.emplace_back(
        DB::serialize(DB::TRANSACTION_HASH_TO_TRANSACTION_INFO_PREFIX, transaction.transactionHash, transaction));
    rawDataToInsert.emplace_back(
        DB::serialize(DB::TRANSACTION_HASH_TO_TRANSACTION_INFO_PREFIX, DB::TRANSACTIONS_COUNT_KEY, totalTxsCount));
    return *this;
}

BlockchainWriteBatch &BlockchainWriteBatch::insertPaymentId(
    const Crypto::Hash &transactionHash,
    const Crypto::Hash paymentId,
    uint32_t totalTxsCountForPaymentId)
{
    rawDataToInsert.emplace_back(DB::serialize(DB::PAYMENT_ID_TO_TX_HASH_PREFIX, paymentId, totalTxsCountForPaymentId));
    rawDataToInsert.emplace_back(DB::serialize(
        DB::PAYMENT_ID_TO_TX_HASH_PREFIX, std::make_pair(paymentId, totalTxsCountForPaymentId - 1), transactionHash));
    return *this;
}

BlockchainWriteBatch &BlockchainWriteBatch::insertCachedBlock(
    const CachedBlockInfo &block,
    uint32_t blockIndex,
    const std::vector<Crypto::Hash> &blockTxs)
{
    rawDataToInsert.emplace_back(DB::serialize(DB::BLOCK_INDEX_TO_BLOCK_INFO_PREFIX, blockIndex, block));
    rawDataToInsert.emplace_back(DB::serialize(DB::BLOCK_INDEX_TO_TX_HASHES_PREFIX, blockIndex, blockTxs));
    rawDataToInsert.emplace_back(DB::serialize(DB::BLOCK_HASH_TO_BLOCK_INDEX_PREFIX, block.blockHash, blockIndex));
    rawDataToInsert.emplace_back(
        DB::serialize(DB::BLOCK_INDEX_TO_BLOCK_HASH_PREFIX, DB::LAST_BLOCK_INDEX_KEY, blockIndex));
    return *this;
}

BlockchainWriteBatch &BlockchainWriteBatch::insertKeyOutputGlobalIndexes(
    IBlockchainCache::Amount amount,
    const std::vector<PackedOutIndex> &outputs,
    uint32_t totalOutputsCountForAmount)
{
    assert(totalOutputsCountForAmount >= outputs.size());
    rawDataToInsert.reserve(rawDataToInsert.size() + outputs.size() + 1);
    rawDataToInsert.emplace_back(DB::serialize(DB::KEY_OUTPUT_AMOUNT_PREFIX, amount, totalOutputsCountForAmount));
    uint32_t currentOutputId = totalOutputsCountForAmount - static_cast<uint32_t>(outputs.size());

    for (const PackedOutIndex &outIndex : outputs)
    {
        rawDataToInsert.emplace_back(
            DB::serialize(DB::KEY_OUTPUT_AMOUNT_PREFIX, std::make_pair(amount, currentOutputId++), outIndex));
    }

    return *this;
}

BlockchainWriteBatch &BlockchainWriteBatch::insertRawBlock(uint32_t blockIndex, const RawBlock &block)
{
    rawDataToInsert.emplace_back(DB::serialize(DB::BLOCK_INDEX_TO_RAW_BLOCK_PREFIX, blockIndex, block));
    return *this;
}

BlockchainWriteBatch &BlockchainWriteBatch::insertClosestTimestampBlockIndex(uint64_t timestamp, uint32_t blockIndex)
{
    rawDataToInsert.emplace_back(DB::serialize(DB::CLOSEST_TIMESTAMP_BLOCK_INDEX_PREFIX, timestamp, blockIndex));
    return *this;
}

BlockchainWriteBatch &BlockchainWriteBatch::insertKeyOutputAmounts(
    const std::set<IBlockchainCache::Amount> &amounts,
    uint32_t totalKeyOutputAmountsCount)
{
    assert(totalKeyOutputAmountsCount >= amounts.size());
    rawDataToInsert.reserve(rawDataToInsert.size() + amounts.size() + 1);
    rawDataToInsert.emplace_back(DB::serialize(
        DB::KEY_OUTPUT_AMOUNTS_COUNT_PREFIX, DB::KEY_OUTPUT_AMOUNTS_COUNT_KEY, totalKeyOutputAmountsCount));
    uint32_t currentAmountId = totalKeyOutputAmountsCount - static_cast<uint32_t>(amounts.size());

    for (const IBlockchainCache::Amount &amount : amounts)
    {
        rawDataToInsert.emplace_back(DB::serialize(DB::KEY_OUTPUT_AMOUNTS_COUNT_PREFIX, currentAmountId++, amount));
    }

    return *this;
}

BlockchainWriteBatch &
    BlockchainWriteBatch::insertTimestamp(uint64_t timestamp, const std::vector<Crypto::Hash> &blockHashes)
{
    rawDataToInsert.emplace_back(DB::serialize(DB::TIMESTAMP_TO_BLOCKHASHES_PREFIX, timestamp, blockHashes));
    return *this;
}

BlockchainWriteBatch &BlockchainWriteBatch::insertKeyOutputInfo(
    IBlockchainCache::Amount amount,
    IBlockchainCache::GlobalOutputIndex globalIndex,
    const KeyOutputInfo &outputInfo)
{
    rawDataToInsert.emplace_back(
        DB::serialize(DB::KEY_OUTPUT_KEY_PREFIX, std::make_pair(amount, globalIndex), outputInfo));
    return *this;
}

BlockchainWriteBatch &
    BlockchainWriteBatch::removeSpentKeyImages(uint32_t blockIndex, const std::vector<Crypto::KeyImage> &spentKeyImages)
{
    rawKeysToRemove.reserve(rawKeysToRemove.size() + spentKeyImages.size() + 1);
    rawKeysToRemove.emplace_back(DB::serializeKey(DB::BLOCK_INDEX_TO_KEY_IMAGE_PREFIX, blockIndex));

    for (const Crypto::KeyImage &keyImage : spentKeyImages)
    {
        rawKeysToRemove.emplace_back(DB::serializeKey(DB::KEY_IMAGE_TO_BLOCK_INDEX_PREFIX, keyImage));
    }

    return *this;
}

BlockchainWriteBatch &
    BlockchainWriteBatch::removeCachedTransaction(const Crypto::Hash &transactionHash, uint64_t totalTxsCount)
{
    rawKeysToRemove.emplace_back(DB::serializeKey(DB::TRANSACTION_HASH_TO_TRANSACTION_INFO_PREFIX, transactionHash));
    rawDataToInsert.emplace_back(
        DB::serialize(DB::TRANSACTION_HASH_TO_TRANSACTION_INFO_PREFIX, DB::TRANSACTIONS_COUNT_KEY, totalTxsCount));
    return *this;
}

BlockchainWriteBatch &
    BlockchainWriteBatch::removePaymentId(const Crypto::Hash paymentId, uint32_t totalTxsCountForPaymentId)
{
    rawDataToInsert.emplace_back(DB::serialize(DB::PAYMENT_ID_TO_TX_HASH_PREFIX, paymentId, totalTxsCountForPaymentId));
    rawKeysToRemove.emplace_back(
        DB::serializeKey(DB::PAYMENT_ID_TO_TX_HASH_PREFIX, std::make_pair(paymentId, totalTxsCountForPaymentId)));
    return *this;
}

BlockchainWriteBatch &BlockchainWriteBatch::removeCachedBlock(const Crypto::Hash &blockHash, uint32_t blockIndex)
{
    rawKeysToRemove.emplace_back(DB::serializeKey(DB::BLOCK_INDEX_TO_BLOCK_INFO_PREFIX, blockIndex));
    rawKeysToRemove.emplace_back(DB::serializeKey(DB::BLOCK_INDEX_TO_TX_HASHES_PREFIX, blockIndex));
    rawKeysToRemove.emplace_back(DB::serializeKey(DB::BLOCK_HASH_TO_BLOCK_INDEX_PREFIX, blockHash));
    rawDataToInsert.emplace_back(
        DB::serialize(DB::BLOCK_INDEX_TO_BLOCK_HASH_PREFIX, DB::LAST_BLOCK_INDEX_KEY, blockIndex - 1));
    return *this;
}

BlockchainWriteBatch &BlockchainWriteBatch::removeKeyOutputGlobalIndexes(
    IBlockchainCache::Amount amount,
    uint32_t outputsToRemoveCount,
    uint32_t totalOutputsCountForAmount)
{
    rawKeysToRemove.reserve(rawKeysToRemove.size() + outputsToRemoveCount);
    rawDataToInsert.emplace_back(DB::serialize(DB::KEY_OUTPUT_AMOUNT_PREFIX, amount, totalOutputsCountForAmount));
    for (uint32_t i = 0; i < outputsToRemoveCount; ++i)
    {
        rawKeysToRemove.emplace_back(
            DB::serializeKey(DB::KEY_OUTPUT_AMOUNT_PREFIX, std::make_pair(amount, totalOutputsCountForAmount + i)));
    }
    return *this;
}

BlockchainWriteBatch &BlockchainWriteBatch::removeRawBlock(uint32_t blockIndex)
{
    rawKeysToRemove.emplace_back(DB::serializeKey(DB::BLOCK_INDEX_TO_RAW_BLOCK_PREFIX, blockIndex));
    return *this;
}

BlockchainWriteBatch &BlockchainWriteBatch::removeClosestTimestampBlockIndex(uint64_t timestamp)
{
    rawKeysToRemove.emplace_back(DB::serializeKey(DB::CLOSEST_TIMESTAMP_BLOCK_INDEX_PREFIX, timestamp));
    return *this;
}

BlockchainWriteBatch &BlockchainWriteBatch::removeTimestamp(uint64_t timestamp)
{
    rawKeysToRemove.emplace_back(DB::serializeKey(DB::TIMESTAMP_TO_BLOCKHASHES_PREFIX, timestamp));
    return *this;
}

BlockchainWriteBatch &BlockchainWriteBatch::removeKeyOutputInfo(
    IBlockchainCache::Amount amount,
    IBlockchainCache::GlobalOutputIndex globalIndex)
{
    rawKeysToRemove.emplace_back(DB::serializeKey(DB::KEY_OUTPUT_KEY_PREFIX, std::make_pair(amount, globalIndex)));
    return *this;
}

std::vector<std::pair<std::string, std::string>> BlockchainWriteBatch::extractRawDataToInsert()
{
    return std::move(rawDataToInsert);
}

std::vector<std::string> BlockchainWriteBatch::extractRawKeysToRemove()
{
    return std::move(rawKeysToRemove);
}
