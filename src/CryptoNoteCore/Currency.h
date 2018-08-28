// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <boost/utility.hpp>
#include "../CryptoNoteConfig.h"
#include "../crypto/hash.h"
#include "../Logging/LoggerRef.h"
#include "CachedBlock.h"
#include "CryptoNoteBasic.h"

namespace CryptoNote {

class AccountBase;

class Currency {
public:
  uint32_t maxBlockHeight() const { return m_maxBlockHeight; }
  size_t maxBlockBlobSize() const { return m_maxBlockBlobSize; }
  size_t maxTxSize() const { return m_maxTxSize; }
  uint64_t publicAddressBase58Prefix() const { return m_publicAddressBase58Prefix; }
  uint32_t minedMoneyUnlockWindow() const { return m_minedMoneyUnlockWindow; }

  size_t timestampCheckWindow(uint32_t blockHeight) const
  {
      if (blockHeight >= CryptoNote::parameters::LWMA_2_DIFFICULTY_BLOCK_INDEX_V3)
      {
          return CryptoNote::parameters::BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V3;
      }
      else
      {
          return m_timestampCheckWindow;
      }
  }

  uint64_t blockFutureTimeLimit(uint32_t blockHeight) const
  {
      if (blockHeight >= CryptoNote::parameters::LWMA_2_DIFFICULTY_BLOCK_INDEX_V2)
      {
          return CryptoNote::parameters::CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V4;
      }
      else if (blockHeight >= CryptoNote::parameters::LWMA_2_DIFFICULTY_BLOCK_INDEX)
      {
          return CryptoNote::parameters::CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V3;
      }
      else
      {
          return m_blockFutureTimeLimit;
      }
  }

  uint64_t moneySupply() const { return m_moneySupply; }
  unsigned int emissionSpeedFactor() const { return m_emissionSpeedFactor; }
  uint64_t genesisBlockReward() const { return m_genesisBlockReward; }

  size_t rewardBlocksWindow() const { return m_rewardBlocksWindow; }
  uint32_t zawyDifficultyBlockIndex() const { return m_zawyDifficultyBlockIndex; }
  size_t zawyDifficultyV2() const { return m_zawyDifficultyV2; }
  uint8_t zawyDifficultyBlockVersion() const { return m_zawyDifficultyBlockVersion; }
  size_t blockGrantedFullRewardZone() const { return m_blockGrantedFullRewardZone; }
  size_t blockGrantedFullRewardZoneByBlockVersion(uint8_t blockMajorVersion) const;
  size_t minerTxBlobReservedSize() const { return m_minerTxBlobReservedSize; }

  size_t numberOfDecimalPlaces() const { return m_numberOfDecimalPlaces; }
  uint64_t coin() const { return m_coin; }

  uint64_t minimumFee() const { return m_mininumFee; }
  uint64_t defaultDustThreshold(uint32_t height) const {
      if (height >= CryptoNote::parameters::DUST_THRESHOLD_V2_HEIGHT)
      {
          return CryptoNote::parameters::DEFAULT_DUST_THRESHOLD_V2;
      }

      return m_defaultDustThreshold;
  }
  uint64_t defaultFusionDustThreshold(uint32_t height) const {
      if (height >= CryptoNote::parameters::FUSION_DUST_THRESHOLD_HEIGHT_V2)
      {
          return CryptoNote::parameters::DEFAULT_DUST_THRESHOLD_V2;
      }

      return m_defaultDustThreshold;
  }

  uint64_t difficultyTarget() const { return m_difficultyTarget; }
  size_t difficultyWindow() const { return m_difficultyWindow; }
size_t difficultyWindowByBlockVersion(uint8_t blockMajorVersion) const;
  size_t difficultyLag() const { return m_difficultyLag; }
size_t difficultyLagByBlockVersion(uint8_t blockMajorVersion) const;
  size_t difficultyCut() const { return m_difficultyCut; }
size_t difficultyCutByBlockVersion(uint8_t blockMajorVersion) const;
  size_t difficultyBlocksCount() const { return m_difficultyWindow + m_difficultyLag; }
size_t difficultyBlocksCountByBlockVersion(uint8_t blockMajorVersion, uint32_t height) const;

  size_t maxBlockSizeInitial() const { return m_maxBlockSizeInitial; }
  uint64_t maxBlockSizeGrowthSpeedNumerator() const { return m_maxBlockSizeGrowthSpeedNumerator; }
  uint64_t maxBlockSizeGrowthSpeedDenominator() const { return m_maxBlockSizeGrowthSpeedDenominator; }

  uint64_t lockedTxAllowedDeltaSeconds() const { return m_lockedTxAllowedDeltaSeconds; }
  size_t lockedTxAllowedDeltaBlocks() const { return m_lockedTxAllowedDeltaBlocks; }

  uint64_t mempoolTxLiveTime() const { return m_mempoolTxLiveTime; }
  uint64_t mempoolTxFromAltBlockLiveTime() const { return m_mempoolTxFromAltBlockLiveTime; }
  uint64_t numberOfPeriodsToForgetTxDeletedFromPool() const { return m_numberOfPeriodsToForgetTxDeletedFromPool; }

  size_t fusionTxMaxSize() const { return m_fusionTxMaxSize; }
  size_t fusionTxMinInputCount() const { return m_fusionTxMinInputCount; }
  size_t fusionTxMinInOutCountRatio() const { return m_fusionTxMinInOutCountRatio; }

  uint32_t upgradeHeight(uint8_t majorVersion) const;
  unsigned int upgradeVotingThreshold() const { return m_upgradeVotingThreshold; }
  uint32_t upgradeVotingWindow() const { return m_upgradeVotingWindow; }
  uint32_t upgradeWindow() const { return m_upgradeWindow; }
  uint32_t minNumberVotingBlocks() const { return (m_upgradeVotingWindow * m_upgradeVotingThreshold + 99) / 100; }
  uint32_t maxUpgradeDistance() const { return 7 * m_upgradeWindow; }
  uint32_t calculateUpgradeHeight(uint32_t voteCompleteHeight) const { return voteCompleteHeight + m_upgradeWindow; }

  const std::string& blocksFileName() const { return m_blocksFileName; }
  const std::string& blockIndexesFileName() const { return m_blockIndexesFileName; }
  const std::string& txPoolFileName() const { return m_txPoolFileName; }

  bool isBlockexplorer() const { return m_isBlockexplorer; }
  bool isTestnet() const { return m_testnet; }

  const BlockTemplate& genesisBlock() const { return cachedGenesisBlock->getBlock(); }
  const Crypto::Hash& genesisBlockHash() const { return cachedGenesisBlock->getBlockHash(); }

  bool getBlockReward(uint8_t blockMajorVersion, size_t medianSize, size_t currentBlockSize, uint64_t alreadyGeneratedCoins, uint64_t fee,
    uint64_t& reward, int64_t& emissionChange) const;
  size_t maxBlockCumulativeSize(uint64_t height) const;

  bool constructMinerTx(uint8_t blockMajorVersion, uint32_t height, size_t medianSize, uint64_t alreadyGeneratedCoins, size_t currentBlockSize,
    uint64_t fee, const AccountPublicAddress& minerAddress, Transaction& tx, const BinaryArray& extraNonce = BinaryArray(), size_t maxOuts = 1) const;

  bool isFusionTransaction(const Transaction& transaction, uint32_t height) const;
  bool isFusionTransaction(const Transaction& transaction, size_t size, uint32_t height) const;
  bool isFusionTransaction(const std::vector<uint64_t>& inputsAmounts, const std::vector<uint64_t>& outputsAmounts, size_t size, uint32_t height) const;
  bool isAmountApplicableInFusionTransactionInput(uint64_t amount, uint64_t threshold, uint32_t height) const;
  bool isAmountApplicableInFusionTransactionInput(uint64_t amount, uint64_t threshold, uint8_t& amountPowerOfTen, uint32_t height) const;

  std::string accountAddressAsString(const AccountBase& account) const;
  std::string accountAddressAsString(const AccountPublicAddress& accountPublicAddress) const;
  bool parseAccountAddressString(const std::string& str, AccountPublicAddress& addr) const;

  std::string formatAmount(uint64_t amount) const;
  std::string formatAmount(int64_t amount) const;
  bool parseAmount(const std::string& str, uint64_t& amount) const;

  uint64_t getNextDifficulty(uint8_t version, uint32_t blockIndex, std::vector<uint64_t> timestamps, std::vector<uint64_t> cumulativeDifficulties) const;
  uint64_t nextDifficulty(uint8_t version, uint32_t blockIndex, std::vector<uint64_t> timestamps, std::vector<uint64_t> cumulativeDifficulties) const;


  bool checkProofOfWorkV1(const CachedBlock& block, uint64_t currentDifficulty) const;
  bool checkProofOfWorkV2(const CachedBlock& block, uint64_t currentDifficulty) const;
  bool checkProofOfWork(const CachedBlock& block, uint64_t currentDifficulty) const;

  Currency(Currency&& currency);

  size_t getApproximateMaximumInputCount(size_t transactionSize, size_t outputCount, size_t mixinCount) const;

private:
  Currency(Logging::ILogger& log) : logger(log, "currency") {
  }

  bool init();

  bool generateGenesisBlock();

private:
  uint32_t m_maxBlockHeight;
  size_t m_maxBlockBlobSize;
  size_t m_maxTxSize;
  uint64_t m_publicAddressBase58Prefix;
  uint32_t m_minedMoneyUnlockWindow;

  size_t m_timestampCheckWindow;
  uint64_t m_blockFutureTimeLimit;

  uint64_t m_moneySupply;
  unsigned int m_emissionSpeedFactor;
  uint64_t m_genesisBlockReward;

  size_t m_rewardBlocksWindow;
  uint32_t m_zawyDifficultyBlockIndex;
  size_t m_zawyDifficultyV2;
  uint8_t m_zawyDifficultyBlockVersion;
  size_t m_blockGrantedFullRewardZone;
  size_t m_minerTxBlobReservedSize;

  size_t m_numberOfDecimalPlaces;
  uint64_t m_coin;

  uint64_t m_mininumFee;
  uint64_t m_defaultDustThreshold;

  uint64_t m_difficultyTarget;
  size_t m_difficultyWindow;
  size_t m_difficultyLag;
  size_t m_difficultyCut;

  size_t m_maxBlockSizeInitial;
  uint64_t m_maxBlockSizeGrowthSpeedNumerator;
  uint64_t m_maxBlockSizeGrowthSpeedDenominator;

  uint64_t m_lockedTxAllowedDeltaSeconds;
  size_t m_lockedTxAllowedDeltaBlocks;

  uint64_t m_mempoolTxLiveTime;
  uint64_t m_mempoolTxFromAltBlockLiveTime;
  uint64_t m_numberOfPeriodsToForgetTxDeletedFromPool;

  size_t m_fusionTxMaxSize;
  size_t m_fusionTxMinInputCount;
  size_t m_fusionTxMinInOutCountRatio;

  uint32_t m_upgradeHeightV2;
  uint32_t m_upgradeHeightV3;
  uint32_t m_upgradeHeightV4;
  unsigned int m_upgradeVotingThreshold;
  uint32_t m_upgradeVotingWindow;
  uint32_t m_upgradeWindow;

  std::string m_blocksFileName;
  std::string m_blockIndexesFileName;
  std::string m_txPoolFileName;

  static const std::vector<uint64_t> PRETTY_AMOUNTS;

  bool m_testnet;
  bool m_isBlockexplorer;

  BlockTemplate genesisBlockTemplate;
  std::unique_ptr<CachedBlock> cachedGenesisBlock;

  Logging::LoggerRef logger;

  friend class CurrencyBuilder;
};

class CurrencyBuilder : boost::noncopyable {
public:
  CurrencyBuilder(Logging::ILogger& log);

  Currency currency() {
    if (!m_currency.init()) {
      throw std::runtime_error("Failed to initialize currency object");
    }

    return std::move(m_currency);
  }

  Transaction generateGenesisTransaction();
  Transaction generateGenesisTransaction(const std::vector<AccountPublicAddress>& targets);
  CurrencyBuilder& maxBlockNumber(uint32_t val) { m_currency.m_maxBlockHeight = val; return *this; }
  CurrencyBuilder& maxBlockBlobSize(size_t val) { m_currency.m_maxBlockBlobSize = val; return *this; }
  CurrencyBuilder& maxTxSize(size_t val) { m_currency.m_maxTxSize = val; return *this; }
  CurrencyBuilder& publicAddressBase58Prefix(uint64_t val) { m_currency.m_publicAddressBase58Prefix = val; return *this; }
  CurrencyBuilder& minedMoneyUnlockWindow(uint32_t val) { m_currency.m_minedMoneyUnlockWindow = val; return *this; }

  CurrencyBuilder& timestampCheckWindow(size_t val) { m_currency.m_timestampCheckWindow = val; return *this; }
  CurrencyBuilder& blockFutureTimeLimit(uint64_t val) { m_currency.m_blockFutureTimeLimit = val; return *this; }

  CurrencyBuilder& moneySupply(uint64_t val) { m_currency.m_moneySupply = val; return *this; }
  CurrencyBuilder& emissionSpeedFactor(unsigned int val);
  CurrencyBuilder& genesisBlockReward(uint64_t val) { m_currency.m_genesisBlockReward = val; return *this; }

  CurrencyBuilder& rewardBlocksWindow(size_t val) { m_currency.m_rewardBlocksWindow = val; return *this; }
  CurrencyBuilder& zawyDifficultyBlockIndex(uint32_t val) { m_currency.m_zawyDifficultyBlockIndex = val; return *this; }
  CurrencyBuilder& zawyDifficultyV2(size_t val) { m_currency.m_zawyDifficultyV2 = val; return *this; }
  CurrencyBuilder& zawyDifficultyBlockVersion(uint8_t val) { m_currency.m_zawyDifficultyBlockVersion = val; return *this; }
  CurrencyBuilder& blockGrantedFullRewardZone(size_t val) { m_currency.m_blockGrantedFullRewardZone = val; return *this; }
  CurrencyBuilder& minerTxBlobReservedSize(size_t val) { m_currency.m_minerTxBlobReservedSize = val; return *this; }

  CurrencyBuilder& numberOfDecimalPlaces(size_t val);

  CurrencyBuilder& mininumFee(uint64_t val) { m_currency.m_mininumFee = val; return *this; }
  CurrencyBuilder& defaultDustThreshold(uint64_t val) { m_currency.m_defaultDustThreshold = val; return *this; }

  CurrencyBuilder& difficultyTarget(uint64_t val) { m_currency.m_difficultyTarget = val; return *this; }
  CurrencyBuilder& difficultyWindow(size_t val);
  CurrencyBuilder& difficultyLag(size_t val) { m_currency.m_difficultyLag = val; return *this; }
  CurrencyBuilder& difficultyCut(size_t val) { m_currency.m_difficultyCut = val; return *this; }

  CurrencyBuilder& maxBlockSizeInitial(size_t val) { m_currency.m_maxBlockSizeInitial = val; return *this; }
  CurrencyBuilder& maxBlockSizeGrowthSpeedNumerator(uint64_t val) { m_currency.m_maxBlockSizeGrowthSpeedNumerator = val; return *this; }
  CurrencyBuilder& maxBlockSizeGrowthSpeedDenominator(uint64_t val) { m_currency.m_maxBlockSizeGrowthSpeedDenominator = val; return *this; }

  CurrencyBuilder& lockedTxAllowedDeltaSeconds(uint64_t val) { m_currency.m_lockedTxAllowedDeltaSeconds = val; return *this; }
  CurrencyBuilder& lockedTxAllowedDeltaBlocks(size_t val) { m_currency.m_lockedTxAllowedDeltaBlocks = val; return *this; }

  CurrencyBuilder& mempoolTxLiveTime(uint64_t val) { m_currency.m_mempoolTxLiveTime = val; return *this; }
  CurrencyBuilder& mempoolTxFromAltBlockLiveTime(uint64_t val) { m_currency.m_mempoolTxFromAltBlockLiveTime = val; return *this; }
  CurrencyBuilder& numberOfPeriodsToForgetTxDeletedFromPool(uint64_t val) { m_currency.m_numberOfPeriodsToForgetTxDeletedFromPool = val; return *this; }

  CurrencyBuilder& fusionTxMaxSize(size_t val) { m_currency.m_fusionTxMaxSize = val; return *this; }
  CurrencyBuilder& fusionTxMinInputCount(size_t val) { m_currency.m_fusionTxMinInputCount = val; return *this; }
  CurrencyBuilder& fusionTxMinInOutCountRatio(size_t val) { m_currency.m_fusionTxMinInOutCountRatio = val; return *this; }

  CurrencyBuilder& upgradeHeightV2(uint32_t val) { m_currency.m_upgradeHeightV2 = val; return *this; }
  CurrencyBuilder& upgradeHeightV3(uint32_t val) { m_currency.m_upgradeHeightV3 = val; return *this; }
  CurrencyBuilder& upgradeHeightV4(uint32_t val) { m_currency.m_upgradeHeightV4 = val; return *this; }
  CurrencyBuilder& upgradeVotingThreshold(unsigned int val);
  CurrencyBuilder& upgradeVotingWindow(uint32_t val) { m_currency.m_upgradeVotingWindow = val; return *this; }
  CurrencyBuilder& upgradeWindow(uint32_t val);

  CurrencyBuilder& blocksFileName(const std::string& val) { m_currency.m_blocksFileName = val; return *this; }
  CurrencyBuilder& blockIndexesFileName(const std::string& val) { m_currency.m_blockIndexesFileName = val; return *this; }
  CurrencyBuilder& txPoolFileName(const std::string& val) { m_currency.m_txPoolFileName = val; return *this; }
  
  CurrencyBuilder& isBlockexplorer(const bool val) { m_currency.m_isBlockexplorer = val; return *this; }
  CurrencyBuilder& testnet(bool val) { m_currency.m_testnet = val; return *this; }

private:
  Currency m_currency;
};

}
