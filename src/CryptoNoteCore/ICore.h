// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>
#include <CryptoNote.h>

#include "AddBlockErrors.h"
#include "AddBlockErrorCondition.h"
#include "BlockchainExplorerData.h"
#include "BlockchainMessages.h"
#include "CachedBlock.h"
#include "CachedTransaction.h"
#include "CoreStatistics.h"
#include "ICoreObserver.h"
#include "ICoreDefinitions.h"
#include "MessageQueue.h"

namespace CryptoNote {

enum class CoreEvent { POOL_UPDATED, BLOCKHAIN_UPDATED };

class ICore {
public:
  virtual ~ICore() {
  }

  virtual bool addMessageQueue(MessageQueue<BlockchainMessage>& messageQueue) = 0;
  virtual bool removeMessageQueue(MessageQueue<BlockchainMessage>& messageQueue) = 0;

  virtual uint32_t getTopBlockIndex() const = 0;
  virtual Crypto::Hash getTopBlockHash() const = 0;
  virtual Crypto::Hash getBlockHashByIndex(uint32_t blockIndex) const = 0;
  virtual uint64_t getBlockTimestampByIndex(uint32_t blockIndex) const = 0;

  virtual bool hasBlock(const Crypto::Hash& blockHash) const = 0;
  virtual BlockTemplate getBlockByIndex(uint32_t index) const = 0;
  virtual BlockTemplate getBlockByHash(const Crypto::Hash& blockHash) const = 0;

  virtual std::vector<Crypto::Hash> buildSparseChain() const = 0;
  virtual std::vector<Crypto::Hash> findBlockchainSupplement(const std::vector<Crypto::Hash>& remoteBlockIds,
                                                             size_t maxCount, uint32_t& totalBlockCount,
                                                             uint32_t& startBlockIndex) const = 0;

  virtual std::vector<RawBlock> getBlocks(uint32_t startIndex, uint32_t count) const = 0;
  virtual void getBlocks(const std::vector<Crypto::Hash>& blockHashes, std::vector<RawBlock>& blocks,
                         std::vector<Crypto::Hash>& missedHashes) const = 0;
  virtual bool queryBlocks(const std::vector<Crypto::Hash>& blockHashes, uint64_t timestamp, uint32_t& startIndex,
                           uint32_t& currentIndex, uint32_t& fullOffset, std::vector<BlockFullInfo>& entries) const = 0;
  virtual bool queryBlocksLite(const std::vector<Crypto::Hash>& knownBlockHashes, uint64_t timestamp,
                               uint32_t& startIndex, uint32_t& currentIndex, uint32_t& fullOffset,
                               std::vector<BlockShortInfo>& entries) const = 0;
  virtual bool queryBlocksDetailed(const std::vector<Crypto::Hash>& knownBlockHashes, uint64_t timestamp,
                              uint64_t& startIndex, uint64_t& currentIndex, uint64_t& fullOffset,
                              std::vector<BlockDetails>& entries, uint32_t blockCount) const = 0;

  virtual bool getWalletSyncData(
    const std::vector<Crypto::Hash> &knownBlockHashes,
    const uint64_t startHeight,
    const uint64_t startTimestamp,
    const uint64_t blockCount,
    std::vector<WalletTypes::WalletBlockInfo> &blocks) const = 0;

  virtual bool getTransactionsStatus(
    std::unordered_set<Crypto::Hash> transactionHashes,
    std::unordered_set<Crypto::Hash> &transactionsInPool,
    std::unordered_set<Crypto::Hash> &transactionsInBlock,
    std::unordered_set<Crypto::Hash> &transactionsUnknown) const = 0;

  virtual bool hasTransaction(const Crypto::Hash& transactionHash) const = 0;
  /*!
   * \brief getTransaction Queries a single transaction details blob from the chain or transaction pool
   * \param hash The hash of the transaction
   * \return The binary blob of the queried transaction, or none if the transaction does not exist.
   */
  virtual std::optional<BinaryArray> getTransaction(const Crypto::Hash& hash) const = 0;
  virtual void getTransactions(const std::vector<Crypto::Hash>& transactionHashes,
                               std::vector<BinaryArray>& transactions,
                               std::vector<Crypto::Hash>& missedHashes) const = 0;

  virtual uint64_t getBlockDifficulty(uint32_t blockIndex) const = 0;
  virtual uint64_t getDifficultyForNextBlock() const = 0;

  virtual std::error_code addBlock(const CachedBlock& cachedBlock, RawBlock&& rawBlock) = 0;
  virtual std::error_code addBlock(RawBlock&& rawBlock) = 0;

  virtual std::error_code submitBlock(BinaryArray&& rawBlockTemplate) = 0;

  virtual bool getTransactionGlobalIndexes(const Crypto::Hash& transactionHash,
                                           std::vector<uint32_t>& globalIndexes) const = 0;
  virtual bool getRandomOutputs(uint64_t amount, uint16_t count, std::vector<uint32_t>& globalIndexes,
                                std::vector<Crypto::PublicKey>& publicKeys) const = 0;

  virtual bool getGlobalIndexesForRange(
    const uint64_t startHeight,
    const uint64_t endHeight,
    std::unordered_map<Crypto::Hash, std::vector<uint64_t>> &indexes) const = 0;

  virtual bool addTransactionToPool(const BinaryArray& transactionBinaryArray) = 0;

  virtual std::vector<Crypto::Hash> getPoolTransactionHashes() const = 0;
  virtual std::tuple<bool, CryptoNote::BinaryArray> getPoolTransaction(const Crypto::Hash& transactionHash) const = 0;
  virtual bool getPoolChanges(const Crypto::Hash& lastBlockHash, const std::vector<Crypto::Hash>& knownHashes,
                              std::vector<BinaryArray>& addedTransactions,
                              std::vector<Crypto::Hash>& deletedTransactions) const = 0;
  virtual bool getPoolChangesLite(const Crypto::Hash& lastBlockHash, const std::vector<Crypto::Hash>& knownHashes,
                                  std::vector<TransactionPrefixInfo>& addedTransactions,
                                  std::vector<Crypto::Hash>& deletedTransactions) const = 0;

  virtual bool getBlockTemplate(BlockTemplate& b, const AccountPublicAddress& adr, const BinaryArray& extraNonce,
                                uint64_t& difficulty, uint32_t& height) const = 0;

  virtual CoreStatistics getCoreStatistics() const = 0;

  virtual void save() = 0;
  virtual void load() = 0;

  virtual BlockDetails getBlockDetails(const Crypto::Hash& blockHash) const = 0;
  virtual TransactionDetails getTransactionDetails(const Crypto::Hash& transactionHash) const = 0;
  virtual std::vector<Crypto::Hash> getBlockHashesByTimestamps(uint64_t timestampBegin, size_t secondsCount) const = 0;
  virtual std::vector<Crypto::Hash> getTransactionHashesByPaymentId(const Crypto::Hash& paymentId) const = 0;
};
}
