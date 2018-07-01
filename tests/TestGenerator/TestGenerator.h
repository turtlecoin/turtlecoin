// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>
#include <list>
#include <vector>
#include <unordered_map>

#include "crypto/hash.h"
#include "CryptoNoteCore/CachedBlock.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/Difficulty.h"


class test_generator
{
public:
  struct BlockInfo {
    BlockInfo()
      : previousBlockHash()
      , alreadyGeneratedCoins(0)
      , blockSize(0) {
    }

    BlockInfo(Crypto::Hash aPrevId, uint64_t anAlreadyGeneratedCoins, size_t aBlockSize)
      : previousBlockHash(aPrevId)
      , alreadyGeneratedCoins(anAlreadyGeneratedCoins)
      , blockSize(aBlockSize) {
    }

    Crypto::Hash previousBlockHash;
    uint64_t alreadyGeneratedCoins;
    size_t blockSize;
  };

  enum BlockFields {
    bf_none      = 0,
    bf_major_ver = 1 << 0,
    bf_minor_ver = 1 << 1,
    bf_timestamp = 1 << 2,
    bf_prev_id   = 1 << 3,
    bf_miner_tx  = 1 << 4,
    bf_tx_hashes = 1 << 5,
    bf_diffic    = 1 << 6
  };

  test_generator(const CryptoNote::Currency& currency, uint8_t majorVersion = CryptoNote::BLOCK_MAJOR_VERSION_1,
                 uint8_t minorVersion = CryptoNote::BLOCK_MINOR_VERSION_0)
      : m_currency(currency), defaultMajorVersion(majorVersion), defaultMinorVersion(minorVersion) {
    std::vector<size_t> unused;
    //genesis block
    addBlock(CryptoNote::CachedBlock(currency.genesisBlock()), 0, 0, unused, 0);
  }

  uint8_t defaultMajorVersion;
  uint8_t defaultMinorVersion;

  const CryptoNote::Currency& currency() const { return m_currency; }

  void getBlockchain(std::vector<BlockInfo>& blockchain, const Crypto::Hash& head, size_t n) const;
  void getLastNBlockSizes(std::vector<size_t>& blockSizes, const Crypto::Hash& head, size_t n) const;
  uint64_t getAlreadyGeneratedCoins(const Crypto::Hash& blockId) const;
  uint64_t getAlreadyGeneratedCoins(const CryptoNote::BlockTemplate& blk) const;

  void addBlock(const CryptoNote::CachedBlock& blk, size_t tsxSize, uint64_t fee, std::vector<size_t>& blockSizes,
    uint64_t alreadyGeneratedCoins);
  bool constructBlock(CryptoNote::BlockTemplate& blk, uint32_t height, const Crypto::Hash& previousBlockHash,
    const CryptoNote::AccountBase& minerAcc, uint64_t timestamp, uint64_t alreadyGeneratedCoins,
    std::vector<size_t>& blockSizes, const std::list<CryptoNote::Transaction>& txList);
  bool constructBlock(CryptoNote::BlockTemplate& blk, const CryptoNote::AccountBase& minerAcc, uint64_t timestamp);
  bool constructBlock(CryptoNote::BlockTemplate& blk, const CryptoNote::BlockTemplate& blkPrev, const CryptoNote::AccountBase& minerAcc,
    const std::list<CryptoNote::Transaction>& txList = std::list<CryptoNote::Transaction>());

  bool constructBlockManually(CryptoNote::BlockTemplate& blk, const CryptoNote::BlockTemplate& prevBlock,
    const CryptoNote::AccountBase& minerAcc, int actualParams = bf_none, uint8_t majorVer = 0,
    uint8_t minorVer = 0, uint64_t timestamp = 0, const Crypto::Hash& previousBlockHash = Crypto::Hash(),
    const CryptoNote::Difficulty& diffic = 1, const CryptoNote::Transaction& baseTransaction = CryptoNote::Transaction(),
    const std::vector<Crypto::Hash>& transactionHashes = std::vector<Crypto::Hash>(), size_t txsSizes = 0, uint64_t fee = 0);
  bool constructBlockManuallyTx(CryptoNote::BlockTemplate& blk, const CryptoNote::BlockTemplate& prevBlock,
    const CryptoNote::AccountBase& minerAcc, const std::vector<Crypto::Hash>& transactionHashes, size_t txsSize);
  bool constructMaxSizeBlock(CryptoNote::BlockTemplate& blk, const CryptoNote::BlockTemplate& blkPrev,
    const CryptoNote::AccountBase& minerAccount, size_t medianBlockCount = 0,
    const std::list<CryptoNote::Transaction>& txList = std::list<CryptoNote::Transaction>());

private:
  const CryptoNote::Currency& m_currency;
  std::unordered_map<Crypto::Hash, BlockInfo> m_blocksInfo;
};

inline CryptoNote::Difficulty getTestDifficulty() { return 1; }
void fillNonce(CryptoNote::BlockTemplate& blk, const CryptoNote::Difficulty& diffic);

bool constructMinerTxManually(const CryptoNote::Currency& currency, uint8_t blockMajorVersion, uint32_t height, uint64_t alreadyGeneratedCoins,
  const CryptoNote::AccountPublicAddress& minerAddress, CryptoNote::Transaction& tx, uint64_t fee, CryptoNote::KeyPair* pTxKey = 0);
bool constructMinerTxBySize(const CryptoNote::Currency& currency, CryptoNote::Transaction& baseTransaction, uint8_t blockMajorVersion, uint32_t height,
  uint64_t alreadyGeneratedCoins, const CryptoNote::AccountPublicAddress& minerAddress,
  std::vector<size_t>& blockSizes, size_t targetTxSize, size_t targetBlockSize, uint64_t fee = 0);
