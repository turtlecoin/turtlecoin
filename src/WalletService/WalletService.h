// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <System/ContextGroup.h>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include "IWallet.h"
#include "INode.h"
#include "CryptoNoteCore/Currency.h"
#include "PaymentServiceJsonRpcMessages.h"
#undef ERROR //TODO: workaround for windows build. fix it
#include "Logging/LoggerRef.h"

#include <fstream>
#include <memory>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/hashed_index.hpp>

namespace CryptoNote {
class IFusionManager;
}

namespace PaymentService {

struct WalletConfiguration {
  std::string walletFile;
  std::string walletPassword;
  bool syncFromZero;
  std::string secretViewKey;
  std::string secretSpendKey;
  std::string mnemonicSeed;
  uint64_t scanHeight;
};

void generateNewWallet(const CryptoNote::Currency& currency, const WalletConfiguration& conf, Logging::ILogger& logger, System::Dispatcher& dispatcher);

struct TransactionsInBlockInfoFilter;

class WalletService {
public:
  WalletService(const CryptoNote::Currency& currency, System::Dispatcher& sys, CryptoNote::INode& node, CryptoNote::IWallet& wallet,
    CryptoNote::IFusionManager& fusionManager, const WalletConfiguration& conf, Logging::ILogger& logger);
  virtual ~WalletService();

  void init();
  void saveWallet();

  std::error_code saveWalletNoThrow();
  std::error_code exportWallet(const std::string& fileName);
  std::error_code resetWallet(const uint64_t scanHeight);
  std::error_code replaceWithNewWallet(const std::string& viewSecretKey, const uint64_t scanHeight, const bool newAddress);
  std::error_code createAddress(const std::string& spendSecretKeyText, const uint64_t scanHeight, const bool newAddress, std::string& address);
  std::error_code createAddressList(const std::vector<std::string>& spendSecretKeysText, const uint64_t scanHeight, const bool newAddress, std::vector<std::string>& addresses);
  std::error_code createAddress(std::string& address);
  std::error_code createTrackingAddress(const std::string& spendPublicKeyText, uint64_t scanHeight, bool newAddress, std::string& address);
  std::error_code deleteAddress(const std::string& address);
  std::error_code getSpendkeys(const std::string& address, std::string& publicSpendKeyText, std::string& secretSpendKeyText);
  std::error_code getBalance(const std::string& address, uint64_t& availableBalance, uint64_t& lockedAmount);
  std::error_code getBalance(uint64_t& availableBalance, uint64_t& lockedAmount);
  std::error_code getBlockHashes(uint32_t firstBlockIndex, uint32_t blockCount, std::vector<std::string>& blockHashes);
  std::error_code getViewKey(std::string& viewSecretKey);
  std::error_code getMnemonicSeed(const std::string& address, std::string& mnemonicSeed);
  std::error_code getTransactionHashes(const std::vector<std::string>& addresses, const std::string& blockHash,
    uint32_t blockCount, const std::string& paymentId, std::vector<TransactionHashesInBlockRpcInfo>& transactionHashes);
  std::error_code getTransactionHashes(const std::vector<std::string>& addresses, uint32_t firstBlockIndex,
    uint32_t blockCount, const std::string& paymentId, std::vector<TransactionHashesInBlockRpcInfo>& transactionHashes);
  std::error_code getTransactions(const std::vector<std::string>& addresses, const std::string& blockHash,
    uint32_t blockCount, const std::string& paymentId, std::vector<TransactionsInBlockRpcInfo>& transactionHashes);
  std::error_code getTransactions(const std::vector<std::string>& addresses, uint32_t firstBlockIndex,
    uint32_t blockCount, const std::string& paymentId, std::vector<TransactionsInBlockRpcInfo>& transactionHashes);
  std::error_code getTransaction(const std::string& transactionHash, TransactionRpcInfo& transaction);
  std::error_code getAddresses(std::vector<std::string>& addresses);
  std::error_code sendTransaction(SendTransaction::Request& request, std::string& transactionHash);
  std::error_code createDelayedTransaction(const CreateDelayedTransaction::Request& request, std::string& transactionHash);
  std::error_code getDelayedTransactionHashes(std::vector<std::string>& transactionHashes);
  std::error_code deleteDelayedTransaction(const std::string& transactionHash);
  std::error_code sendDelayedTransaction(const std::string& transactionHash);
  std::error_code getUnconfirmedTransactionHashes(const std::vector<std::string>& addresses, std::vector<std::string>& transactionHashes);
  std::error_code getStatus(uint32_t& blockCount, uint32_t& knownBlockCount, std::string& lastBlockHash, uint32_t& peerCount);
  std::error_code sendFusionTransaction(uint64_t threshold, uint32_t anonymity, const std::vector<std::string>& addresses,
    const std::string& destinationAddress, std::string& transactionHash);
  std::error_code estimateFusion(uint64_t threshold, const std::vector<std::string>& addresses, uint32_t& fusionReadyCount, uint32_t& totalOutputCount);
  std::error_code createIntegratedAddress(const std::string& address, const std::string& paymentId, std::string& integratedAddress);
  std::error_code getFeeInfo(std::string& address, uint32_t& amount);
  uint64_t getDefaultMixin() const;


private:
  void refresh();
  void reset(const uint64_t scanHeight);

  void loadWallet();
  void loadTransactionIdIndex();
  void getNodeFee();

  void replaceWithNewWallet(const Crypto::SecretKey& viewSecretKey, const uint64_t scanHeight, const bool newAddress);

  std::vector<CryptoNote::TransactionsInBlockInfo> getTransactions(const Crypto::Hash& blockHash, size_t blockCount) const;
  std::vector<CryptoNote::TransactionsInBlockInfo> getTransactions(uint32_t firstBlockIndex, size_t blockCount) const;

  std::vector<TransactionHashesInBlockRpcInfo> getRpcTransactionHashes(const Crypto::Hash& blockHash, size_t blockCount, const TransactionsInBlockInfoFilter& filter) const;
  std::vector<TransactionHashesInBlockRpcInfo> getRpcTransactionHashes(uint32_t firstBlockIndex, size_t blockCount, const TransactionsInBlockInfoFilter& filter) const;

  std::vector<TransactionsInBlockRpcInfo> getRpcTransactions(const Crypto::Hash& blockHash, size_t blockCount, const TransactionsInBlockInfoFilter& filter) const;
  std::vector<TransactionsInBlockRpcInfo> getRpcTransactions(uint32_t firstBlockIndex, size_t blockCount, const TransactionsInBlockInfoFilter& filter) const;

  const CryptoNote::Currency& currency;
  CryptoNote::IWallet& wallet;
  CryptoNote::IFusionManager& fusionManager;
  CryptoNote::INode& node;
  const WalletConfiguration& config;
  bool inited;
  Logging::LoggerRef logger;
  System::Dispatcher& dispatcher;
  System::Event readyEvent;
  System::ContextGroup refreshContext;
  std::string m_node_address;
  uint32_t m_node_fee;

  std::map<std::string, size_t> transactionIdIndex;
};

} //namespace PaymentService
