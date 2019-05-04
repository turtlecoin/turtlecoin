// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "WalletService.h"


#include <future>
#include <assert.h>
#include <sstream>
#include <unordered_set>
#include <tuple>

#include <boost/filesystem/operations.hpp>

#include <System/Timer.h>
#include <System/InterruptedException.h>
#include "Common/Base58.h"
#include "Common/Util.h"

#include "crypto/crypto.h"
#include "CryptoNote.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/CryptoNoteBasicImpl.h"
#include "Common/CryptoNoteTools.h"
#include "Common/TransactionExtra.h"
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/Mixins.h"

#include <System/EventLock.h>
#include <System/RemoteContext.h>

#include "PaymentServiceJsonRpcMessages.h"
#include "NodeFactory.h"

#include "Wallet/WalletGreen.h"
#include "Wallet/WalletErrors.h"
#include "Wallet/WalletUtils.h"
#include "WalletServiceErrorCategory.h"

#include "Mnemonics/Mnemonics.h"

#include <Utilities/Addresses.h>

namespace PaymentService {

namespace {

bool checkPaymentId(const std::string& paymentId) {
  if (paymentId.size() != 64) {
    return false;
  }

  return std::all_of(paymentId.begin(), paymentId.end(), [] (const char c) {
    if (c >= '0' && c <= '9') {
      return true;
    }

    if (c >= 'a' && c <= 'f') {
      return true;
    }

    if (c >= 'A' && c <= 'F') {
      return true;
    }

    return false;
  });
}

Crypto::Hash parsePaymentId(const std::string& paymentIdStr) {
  if (!checkPaymentId(paymentIdStr)) {
    throw std::system_error(make_error_code(CryptoNote::error::WalletServiceErrorCode::WRONG_PAYMENT_ID_FORMAT));
  }

  Crypto::Hash paymentId;
  bool r = Common::podFromHex(paymentIdStr, paymentId);
  if (r) {}
  assert(r);

  return paymentId;
}

bool getPaymentIdFromExtra(const std::string& binaryString, Crypto::Hash& paymentId) {
  return CryptoNote::getPaymentIdFromTxExtra(Common::asBinaryArray(binaryString), paymentId);
}

std::string getPaymentIdStringFromExtra(const std::string& binaryString) {
  Crypto::Hash paymentId;

  try {
    if (!getPaymentIdFromExtra(binaryString, paymentId)) {
      return std::string();
    }
  } catch (std::exception&) {
    return std::string();
  }

  return Common::podToHex(paymentId);
}

}

struct TransactionsInBlockInfoFilter {
  TransactionsInBlockInfoFilter(const std::vector<std::string>& addressesVec, const std::string& paymentIdStr) {
    addresses.insert(addressesVec.begin(), addressesVec.end());

    if (!paymentIdStr.empty()) {
      paymentId = parsePaymentId(paymentIdStr);
      havePaymentId = true;
    } else {
      havePaymentId = false;
    }
  }

  bool checkTransaction(const CryptoNote::WalletTransactionWithTransfers& transaction) const {
    if (havePaymentId) {
      Crypto::Hash transactionPaymentId;
      if (!getPaymentIdFromExtra(transaction.transaction.extra, transactionPaymentId)) {
        return false;
      }

      if (paymentId != transactionPaymentId) {
        return false;
      }
    }

    if (addresses.empty()) {
      return true;
    }

    bool haveAddress = false;
    for (const CryptoNote::WalletTransfer& transfer: transaction.transfers) {
      if (addresses.find(transfer.address) != addresses.end()) {
        haveAddress = true;
        break;
      }
    }

    return haveAddress;
  }

  std::unordered_set<std::string> addresses;
  bool havePaymentId = false;
  Crypto::Hash paymentId;
};

namespace {

void addPaymentIdToExtra(const std::string& paymentId, std::string& extra) {
  std::vector<uint8_t> extraVector;
  if (!CryptoNote::createTxExtraWithPaymentId(paymentId, extraVector)) {
    throw std::system_error(make_error_code(CryptoNote::error::BAD_PAYMENT_ID));
  }

  std::copy(extraVector.begin(), extraVector.end(), std::back_inserter(extra));
}

void validatePaymentId(const std::string& paymentId, Logging::LoggerRef logger) {
  if (!checkPaymentId(paymentId)) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Can't validate payment id: " << paymentId;
    throw std::system_error(make_error_code(CryptoNote::error::WalletServiceErrorCode::WRONG_PAYMENT_ID_FORMAT));
  }
}



Crypto::Hash parseHash(const std::string& hashString, Logging::LoggerRef logger) {
  Crypto::Hash hash;

  if (!Common::podFromHex(hashString, hash)) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Can't parse hash string " << hashString;
    throw std::system_error(make_error_code(CryptoNote::error::WalletServiceErrorCode::WRONG_HASH_FORMAT));
  }

  return hash;
}

std::vector<CryptoNote::TransactionsInBlockInfo> filterTransactions(
  const std::vector<CryptoNote::TransactionsInBlockInfo>& blocks,
  const TransactionsInBlockInfoFilter& filter) {

  std::vector<CryptoNote::TransactionsInBlockInfo> result;

  for (const auto& block: blocks) {
    CryptoNote::TransactionsInBlockInfo item;
    item.blockHash = block.blockHash;

    for (const auto& transaction: block.transactions) {
      if (transaction.transaction.state != CryptoNote::WalletTransactionState::DELETED && filter.checkTransaction(transaction)) {
        item.transactions.push_back(transaction);
      }
    }

    if (!block.transactions.empty()) {
      result.push_back(std::move(item));
    }
  }

  return result;
}

PaymentService::TransactionRpcInfo convertTransactionWithTransfersToTransactionRpcInfo(
  const CryptoNote::WalletTransactionWithTransfers& transactionWithTransfers) {

  PaymentService::TransactionRpcInfo transactionInfo;

  transactionInfo.state = static_cast<uint8_t>(transactionWithTransfers.transaction.state);
  transactionInfo.transactionHash = Common::podToHex(transactionWithTransfers.transaction.hash);
  transactionInfo.blockIndex = transactionWithTransfers.transaction.blockHeight;
  transactionInfo.timestamp = transactionWithTransfers.transaction.timestamp;
  transactionInfo.isBase = transactionWithTransfers.transaction.isBase;
  transactionInfo.unlockTime = transactionWithTransfers.transaction.unlockTime;
  transactionInfo.amount = transactionWithTransfers.transaction.totalAmount;
  transactionInfo.fee = transactionWithTransfers.transaction.fee;
  transactionInfo.extra = Common::toHex(transactionWithTransfers.transaction.extra.data(), transactionWithTransfers.transaction.extra.size());
  transactionInfo.paymentId = getPaymentIdStringFromExtra(transactionWithTransfers.transaction.extra);

  for (const CryptoNote::WalletTransfer& transfer: transactionWithTransfers.transfers) {
    PaymentService::TransferRpcInfo rpcTransfer;
    rpcTransfer.address = transfer.address;
    rpcTransfer.amount = transfer.amount;
    rpcTransfer.type = static_cast<uint8_t>(transfer.type);

    transactionInfo.transfers.push_back(std::move(rpcTransfer));
  }

  return transactionInfo;
}

std::vector<PaymentService::TransactionsInBlockRpcInfo> convertTransactionsInBlockInfoToTransactionsInBlockRpcInfo(
  const std::vector<CryptoNote::TransactionsInBlockInfo>& blocks) {

  std::vector<PaymentService::TransactionsInBlockRpcInfo> rpcBlocks;
  rpcBlocks.reserve(blocks.size());
  for (const auto& block: blocks) {
    PaymentService::TransactionsInBlockRpcInfo rpcBlock;
    rpcBlock.blockHash = Common::podToHex(block.blockHash);

    for (const CryptoNote::WalletTransactionWithTransfers& transactionWithTransfers: block.transactions) {
      PaymentService::TransactionRpcInfo transactionInfo = convertTransactionWithTransfersToTransactionRpcInfo(transactionWithTransfers);
      rpcBlock.transactions.push_back(std::move(transactionInfo));
    }

    rpcBlocks.push_back(std::move(rpcBlock));
  }

  return rpcBlocks;
}

std::vector<PaymentService::TransactionHashesInBlockRpcInfo> convertTransactionsInBlockInfoToTransactionHashesInBlockRpcInfo(
    const std::vector<CryptoNote::TransactionsInBlockInfo>& blocks) {

  std::vector<PaymentService::TransactionHashesInBlockRpcInfo> transactionHashes;
  transactionHashes.reserve(blocks.size());
  for (const CryptoNote::TransactionsInBlockInfo& block: blocks) {
    PaymentService::TransactionHashesInBlockRpcInfo item;
    item.blockHash = Common::podToHex(block.blockHash);

    for (const CryptoNote::WalletTransactionWithTransfers& transaction: block.transactions) {
      item.transactionHashes.emplace_back(Common::podToHex(transaction.transaction.hash));
    }

    transactionHashes.push_back(std::move(item));
  }

  return transactionHashes;
}

void validateAddresses(const std::vector<std::string>& addresses, const CryptoNote::Currency& currency, Logging::LoggerRef logger) {
  for (const auto& address: addresses) {
    if (!CryptoNote::validateAddress(address, currency)) {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Can't validate address " << address;
      throw std::system_error(make_error_code(CryptoNote::error::BAD_ADDRESS));
    }
  }
}

std::tuple<std::string, std::string> decodeIntegratedAddress(const std::string& integratedAddr, const CryptoNote::Currency& currency, Logging::LoggerRef logger) {
    std::string decoded;
    uint64_t prefix;

    /* Need to be able to decode the string as an address */
    if (!Tools::Base58::decode_addr(integratedAddr, prefix, decoded))
    {
        throw std::system_error(make_error_code(CryptoNote::error::BAD_ADDRESS));
    }

    /* The prefix needs to be the same as the base58 prefix */
    if (prefix !=
        CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX)
    {
        throw std::system_error(make_error_code(CryptoNote::error::BAD_ADDRESS));
    }

    const uint64_t paymentIDLen = 64;
    /* Grab the payment ID from the decoded address */
    std::string paymentID = decoded.substr(0, paymentIDLen);

    /* Check the extracted payment ID is good. */
    validatePaymentId(paymentID, logger);

    /* The binary array encoded keys are the rest of the address */
    std::string keys = decoded.substr(paymentIDLen, std::string::npos);

    CryptoNote::AccountPublicAddress addr;
    CryptoNote::BinaryArray ba = Common::asBinaryArray(keys);

    if (!CryptoNote::fromBinaryArray(addr, ba))
    {
        throw std::system_error(make_error_code(CryptoNote::error::BAD_ADDRESS));
    }
    
    /* Parse the AccountPublicAddress into a standard wallet address */
    /* Use the calculated prefix from earlier for less typing :p */
    std::string address = Utilities::getAccountAddressAsStr(prefix, addr);
    
    /* Check the extracted address is good. */
    validateAddresses({address}, currency, logger);
    
    return std::make_tuple(address, paymentID);
}

std::string getValidatedTransactionExtraString(const std::string& extraString) {
  std::vector<uint8_t> binary;
  if (!Common::fromHex(extraString, binary)) {
    throw std::system_error(make_error_code(CryptoNote::error::BAD_TRANSACTION_EXTRA));
  }

  return Common::asString(binary);
}

std::vector<std::string> collectDestinationAddresses(const std::vector<PaymentService::WalletRpcOrder>& orders) {
  std::vector<std::string> result;

  result.reserve(orders.size());
  for (const auto& order: orders) {
    result.push_back(order.address);
  }

  return result;
}

std::vector<CryptoNote::WalletOrder> convertWalletRpcOrdersToWalletOrders(const std::vector<PaymentService::WalletRpcOrder>& orders, const std::string nodeAddress, const uint32_t nodeFee) {
  std::vector<CryptoNote::WalletOrder> result;

  if (!nodeAddress.empty() && nodeFee != 0) {
    result.reserve(orders.size() + 1);
    result.emplace_back(CryptoNote::WalletOrder {nodeAddress, nodeFee});
  } else {
    result.reserve(orders.size());
  }

  for (const auto& order: orders) {
    result.emplace_back(CryptoNote::WalletOrder {order.address, order.amount});
  }

  return result;
}

}

void generateNewWallet(const CryptoNote::Currency& currency, const WalletConfiguration& conf, std::shared_ptr<Logging::ILogger> logger, System::Dispatcher& dispatcher) {
  Logging::LoggerRef log(logger, "generateNewWallet");

  CryptoNote::INode* nodeStub = NodeFactory::createNodeStub();
  std::unique_ptr<CryptoNote::INode> nodeGuard(nodeStub);

  CryptoNote::IWallet* wallet = new CryptoNote::WalletGreen(dispatcher, currency, *nodeStub, logger);
  std::unique_ptr<CryptoNote::IWallet> walletGuard(wallet);

  std::string address;
  if (conf.secretSpendKey.empty() && conf.secretViewKey.empty() && conf.mnemonicSeed.empty())
  {
    log(Logging::INFO, Logging::BRIGHT_WHITE) << "Generating new wallet";

    Crypto::SecretKey private_view_key;
    CryptoNote::KeyPair spendKey;

    Crypto::generate_keys(spendKey.publicKey, spendKey.secretKey);
    Crypto::crypto_ops::generateViewFromSpend(spendKey.secretKey, private_view_key);

    wallet->initializeWithViewKey(conf.walletFile, conf.walletPassword, private_view_key, 0, true);
    address = wallet->createAddress(spendKey.secretKey, 0, true);

	  log(Logging::INFO, Logging::BRIGHT_WHITE) << "New wallet is generated. Address: " << address;
  }
  else if (!conf.mnemonicSeed.empty())
  {
    log(Logging::INFO, Logging::BRIGHT_WHITE) << "Attempting to import wallet from mnemonic seed";

    auto [error, private_spend_key] = Mnemonics::MnemonicToPrivateKey(conf.mnemonicSeed);

    if (error)
    {
        log(Logging::ERROR, Logging::BRIGHT_RED) << error;
        return;
    }

    Crypto::SecretKey private_view_key;

    Crypto::crypto_ops::generateViewFromSpend(private_spend_key, private_view_key);

    wallet->initializeWithViewKey(conf.walletFile, conf.walletPassword, private_view_key, conf.scanHeight, false);

    address = wallet->createAddress(private_spend_key, conf.scanHeight, false);

    log(Logging::INFO, Logging::BRIGHT_WHITE) << "Imported wallet successfully.";
  }
  else
  {
	  if (conf.secretSpendKey.empty() || conf.secretViewKey.empty())
	  {
		  log(Logging::ERROR, Logging::BRIGHT_RED) << "Need both secret spend key and secret view key.";
		  return;
	  }
    else
	  {
		  log(Logging::INFO, Logging::BRIGHT_WHITE) << "Attemping to import wallet from keys";
		  Crypto::Hash private_spend_key_hash;
		  Crypto::Hash private_view_key_hash;
		  uint64_t size;
		  if (!Common::fromHex(conf.secretSpendKey, &private_spend_key_hash, sizeof(private_spend_key_hash), size) || size != sizeof(private_spend_key_hash)) {
			  log(Logging::ERROR, Logging::BRIGHT_RED) << "Invalid spend key";
			  return;
		  }
		  if (!Common::fromHex(conf.secretViewKey, &private_view_key_hash, sizeof(private_view_key_hash), size) || size != sizeof(private_spend_key_hash)) {
			  log(Logging::ERROR, Logging::BRIGHT_RED) << "Invalid view key";
			  return;
		  }
		  Crypto::SecretKey private_spend_key = *(struct Crypto::SecretKey *) &private_spend_key_hash;
		  Crypto::SecretKey private_view_key = *(struct Crypto::SecretKey *) &private_view_key_hash;

		  wallet->initializeWithViewKey(conf.walletFile, conf.walletPassword, private_view_key, conf.scanHeight, false);
		  address = wallet->createAddress(private_spend_key, conf.scanHeight, false);
		  log(Logging::INFO, Logging::BRIGHT_WHITE) << "Imported wallet successfully.";
	  }
  }

  wallet->save(CryptoNote::WalletSaveLevel::SAVE_KEYS_ONLY);
  log(Logging::INFO, Logging::BRIGHT_WHITE) << "Wallet is saved";
}

WalletService::WalletService(const CryptoNote::Currency& currency, System::Dispatcher& sys, CryptoNote::INode& node,
  CryptoNote::IWallet& wallet, CryptoNote::IFusionManager& fusionManager, const WalletConfiguration& conf, std::shared_ptr<Logging::ILogger> logger) :
    currency(currency),
    wallet(wallet),
    fusionManager(fusionManager),
    node(node),
    config(conf),
    inited(false),
    logger(logger, "WalletService"),
    dispatcher(sys),
    readyEvent(dispatcher),
    refreshContext(dispatcher)
{
  readyEvent.set();
}

WalletService::~WalletService() {
  if (inited) {
    wallet.stop();
    refreshContext.wait();
    wallet.shutdown();
  }
}

void WalletService::init() {
  loadWallet();
  loadTransactionIdIndex();

  getNodeFee();
  refreshContext.spawn([this] { refresh(); });

  inited = true;
}

void WalletService::getNodeFee() {
  logger(Logging::DEBUGGING) <<
    "Trying to retrieve node fee information." << std::endl;

  m_node_address = node.feeAddress();
  m_node_fee = node.feeAmount();

  if (!m_node_address.empty() && m_node_fee != 0) {
    // Partially borrowed from <zedwallet/Tools.h>
    uint32_t div = static_cast<uint32_t>(pow(10, CryptoNote::parameters::CRYPTONOTE_DISPLAY_DECIMAL_POINT));
    uint32_t coins = m_node_fee / div;
    uint32_t cents = m_node_fee % div;
    std::stringstream stream;
    stream << std::setfill('0') << std::setw(CryptoNote::parameters::CRYPTONOTE_DISPLAY_DECIMAL_POINT) << cents;
    std::string amount = std::to_string(coins) + "." + stream.str();

    logger(Logging::INFO, Logging::RED) <<
      "You have connected to a node that charges " <<
      "a fee to send transactions." << std::endl;

    logger(Logging::INFO, Logging::RED) <<
      "The fee for sending transactions is: " <<
      amount << " per transaction." << std::endl ;

    logger(Logging::INFO, Logging::RED) <<
      "If you don't want to pay the node fee, please " <<
      "relaunch this program and specify a different " <<
      "node or run your own." << std::endl;
  }
}

void WalletService::saveWallet() {
  wallet.save();
  logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Wallet is saved";
}

void WalletService::loadWallet() {
  logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Loading wallet";
  wallet.load(config.walletFile, config.walletPassword);
  logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Wallet loading is finished.";
}

void WalletService::loadTransactionIdIndex() {
  transactionIdIndex.clear();

  for (size_t i = 0; i < wallet.getTransactionCount(); ++i) {
    transactionIdIndex.emplace(Common::podToHex(wallet.getTransaction(i).hash), i);
  }
}

std::error_code WalletService::saveWalletNoThrow() {
  try {
    System::EventLock lk(readyEvent);

    logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Saving wallet...";

    if (!inited) {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Save impossible: Wallet Service is not initialized";
      return make_error_code(CryptoNote::error::NOT_INITIALIZED);
    }

    saveWallet();
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while saving wallet: " << x.what();
    return x.code();
  } catch (std::exception& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while saving wallet: " << x.what();
    return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
  }

  return std::error_code();
}

std::error_code WalletService::exportWallet(const std::string& fileName) {
  try {
    System::EventLock lk(readyEvent);

    if (!inited) {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Export impossible: Wallet Service is not initialized";
      return make_error_code(CryptoNote::error::NOT_INITIALIZED);
    }

    boost::filesystem::path walletPath(config.walletFile);
    boost::filesystem::path exportPath = walletPath.parent_path() / fileName;

    logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Exporting wallet to " << exportPath.string();
    wallet.exportWallet(exportPath.string());
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while exporting wallet: " << x.what();
    return x.code();
  } catch (std::exception& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while exporting wallet: " << x.what();
    return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
  }

  return std::error_code();
}

std::error_code WalletService::resetWallet(const uint64_t scanHeight) {
  try {
    System::EventLock lk(readyEvent);

    logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Resetting wallet";

    if (!inited) {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Reset impossible: Wallet Service is not initialized";
      return make_error_code(CryptoNote::error::NOT_INITIALIZED);
    }

    reset(scanHeight);
    logger(Logging::INFO, Logging::BRIGHT_WHITE) << "Wallet has been reset";
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while resetting wallet: " << x.what();
    return x.code();
  } catch (std::exception& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while resetting wallet: " << x.what();
    return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
  }

  return std::error_code();
}

std::error_code WalletService::createAddress(const std::string& spendSecretKeyText, uint64_t scanHeight, bool newAddress, std::string& address) {
  try {
    System::EventLock lk(readyEvent);

    logger(Logging::DEBUGGING) << "Creating address";

    Crypto::SecretKey secretKey;
    if (!Common::podFromHex(spendSecretKeyText, secretKey)) {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Wrong key format: " << spendSecretKeyText;
      return make_error_code(CryptoNote::error::WalletServiceErrorCode::WRONG_KEY_FORMAT);
    }

    address = wallet.createAddress(secretKey, scanHeight, newAddress);
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while creating address: " << x.what();
    return x.code();
  }

  logger(Logging::DEBUGGING) << "Created address " << address;

  return std::error_code();
}

std::error_code WalletService::createAddressList(const std::vector<std::string>& spendSecretKeysText, uint64_t scanHeight, bool newAddress, std::vector<std::string>& addresses) {
  try {
    System::EventLock lk(readyEvent);

    logger(Logging::DEBUGGING) << "Creating " << spendSecretKeysText.size() << " addresses...";

    std::vector<Crypto::SecretKey> secretKeys;
    std::unordered_set<std::string> unique;
    secretKeys.reserve(spendSecretKeysText.size());
    unique.reserve(spendSecretKeysText.size());
    for (auto& keyText : spendSecretKeysText) {
      auto insertResult = unique.insert(keyText);
      if (!insertResult.second) {
        logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Not unique key";
        return make_error_code(CryptoNote::error::WalletServiceErrorCode::DUPLICATE_KEY);
      }

      Crypto::SecretKey key;
      if (!Common::podFromHex(keyText, key)) {
        logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Wrong key format: " << keyText;
        return make_error_code(CryptoNote::error::WalletServiceErrorCode::WRONG_KEY_FORMAT);
      }

      secretKeys.push_back(std::move(key));
    }

    addresses = wallet.createAddressList(secretKeys, scanHeight, newAddress);
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while creating addresses: " << x.what();
    return x.code();
  }

  logger(Logging::DEBUGGING) << "Created " << addresses.size() << " addresses";

  return std::error_code();
}

std::error_code WalletService::createAddress(std::string& address) {
  try {
    System::EventLock lk(readyEvent);

    logger(Logging::DEBUGGING) << "Creating address";

    address = wallet.createAddress();
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while creating address: " << x.what();
    return x.code();
  }

  logger(Logging::DEBUGGING) << "Created address " << address;

  return std::error_code();
}

std::error_code WalletService::createTrackingAddress(const std::string& spendPublicKeyText, uint64_t scanHeight, bool newAddress, std::string& address) {
  try {
    System::EventLock lk(readyEvent);

    logger(Logging::DEBUGGING) << "Creating tracking address";

    Crypto::PublicKey publicKey;
    if (!Common::podFromHex(spendPublicKeyText, publicKey)) {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Wrong key format: " << spendPublicKeyText;
      return make_error_code(CryptoNote::error::WalletServiceErrorCode::WRONG_KEY_FORMAT);
    }

    address = wallet.createAddress(publicKey, scanHeight, true);
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while creating tracking address: " << x.what();
    return x.code();
  }

  logger(Logging::DEBUGGING) << "Created address " << address;
  return std::error_code();
}

std::error_code WalletService::deleteAddress(const std::string& address) {
  try {
    System::EventLock lk(readyEvent);

    logger(Logging::DEBUGGING) << "Delete address request came";
    wallet.deleteAddress(address);
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while deleting address: " << x.what();
    return x.code();
  }

  logger(Logging::DEBUGGING) << "Address " << address << " successfully deleted";
  return std::error_code();
}

std::error_code WalletService::getSpendkeys(const std::string& address, std::string& publicSpendKeyText, std::string& secretSpendKeyText) {
  try {
    System::EventLock lk(readyEvent);

    CryptoNote::KeyPair key = wallet.getAddressSpendKey(address);

    publicSpendKeyText = Common::podToHex(key.publicKey);
    secretSpendKeyText = Common::podToHex(key.secretKey);

  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting spend key: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::getBalance(const std::string& address, uint64_t& availableBalance, uint64_t& lockedAmount) {
  try {
    System::EventLock lk(readyEvent);
    logger(Logging::DEBUGGING) << "Getting balance for address " << address;

    availableBalance = wallet.getActualBalance(address);
    lockedAmount = wallet.getPendingBalance(address);
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting balance: " << x.what();
    return x.code();
  }

  logger(Logging::DEBUGGING) << address << " actual balance: " << availableBalance << ", pending: " << lockedAmount;
  return std::error_code();
}

std::error_code WalletService::getBalance(uint64_t& availableBalance, uint64_t& lockedAmount) {
  try {
    System::EventLock lk(readyEvent);
    logger(Logging::DEBUGGING) << "Getting wallet balance";

    availableBalance = wallet.getActualBalance();
    lockedAmount = wallet.getPendingBalance();
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting balance: " << x.what();
    return x.code();
  }

  logger(Logging::DEBUGGING) << "Wallet actual balance: " << availableBalance << ", pending: " << lockedAmount;
  return std::error_code();
}

std::error_code WalletService::getBlockHashes(uint32_t firstBlockIndex, uint32_t blockCount, std::vector<std::string>& blockHashes) {
  try {
    System::EventLock lk(readyEvent);
    std::vector<Crypto::Hash> hashes = wallet.getBlockHashes(firstBlockIndex, blockCount);

    blockHashes.reserve(hashes.size());
    for (const auto& hash: hashes) {
      blockHashes.push_back(Common::podToHex(hash));
    }
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting block hashes: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::getViewKey(std::string& viewSecretKey) {
  try {
    System::EventLock lk(readyEvent);
    CryptoNote::KeyPair viewKey = wallet.getViewKey();
    viewSecretKey = Common::podToHex(viewKey.secretKey);
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting view key: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::getMnemonicSeed(const std::string& address, std::string& mnemonicSeed) {
  try {
    System::EventLock lk(readyEvent);
    CryptoNote::KeyPair key = wallet.getAddressSpendKey(address);
    CryptoNote::KeyPair viewKey = wallet.getViewKey();

    Crypto::SecretKey deterministic_private_view_key;

    Crypto::crypto_ops::generateViewFromSpend(key.secretKey, deterministic_private_view_key);

    bool deterministic_private_keys = deterministic_private_view_key == viewKey.secretKey;

    if (deterministic_private_keys) {
      mnemonicSeed = Mnemonics::PrivateKeyToMnemonic(key.secretKey);
    } else {
      /* Have to be able to derive view key from spend key to create a mnemonic
         seed, due to being able to generate multiple addresses we can't do
         this in walletd as the default */
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Your private keys are not deterministic and so a mnemonic seed cannot be generated!";
      return make_error_code(CryptoNote::error::WalletServiceErrorCode::KEYS_NOT_DETERMINISTIC);
    }
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting mnemonic seed: " << x.what();
    return x.code();
  }

  return std::error_code();
}

std::error_code WalletService::getTransactionHashes(const std::vector<std::string>& addresses, const std::string& blockHashString,
  uint32_t blockCount, const std::string& paymentId, std::vector<TransactionHashesInBlockRpcInfo>& transactionHashes) {
  try {
    System::EventLock lk(readyEvent);
    validateAddresses(addresses, currency, logger);

    if (!paymentId.empty()) {
      validatePaymentId(paymentId, logger);
    }

    TransactionsInBlockInfoFilter transactionFilter(addresses, paymentId);
    Crypto::Hash blockHash = parseHash(blockHashString, logger);

    transactionHashes = getRpcTransactionHashes(blockHash, blockCount, transactionFilter);
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting transactions: " << x.what();
    return x.code();
  } catch (std::exception& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting transactions: " << x.what();
    return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
  }

  return std::error_code();
}

std::error_code WalletService::getTransactionHashes(const std::vector<std::string>& addresses, uint32_t firstBlockIndex,
  uint32_t blockCount, const std::string& paymentId, std::vector<TransactionHashesInBlockRpcInfo>& transactionHashes) {
  try {
    System::EventLock lk(readyEvent);
    validateAddresses(addresses, currency, logger);

    if (!paymentId.empty()) {
      validatePaymentId(paymentId, logger);
    }

    TransactionsInBlockInfoFilter transactionFilter(addresses, paymentId);
    transactionHashes = getRpcTransactionHashes(firstBlockIndex, blockCount, transactionFilter);

  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting transactions: " << x.what();
    return x.code();
  } catch (std::exception& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting transactions: " << x.what();
    return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
  }

  return std::error_code();
}

std::error_code WalletService::getTransactions(const std::vector<std::string>& addresses, const std::string& blockHashString,
  uint32_t blockCount, const std::string& paymentId, std::vector<TransactionsInBlockRpcInfo>& transactions) {
  try {
    System::EventLock lk(readyEvent);
    validateAddresses(addresses, currency, logger);

    if (!paymentId.empty()) {
      validatePaymentId(paymentId, logger);
    }

    TransactionsInBlockInfoFilter transactionFilter(addresses, paymentId);

    Crypto::Hash blockHash = parseHash(blockHashString, logger);

    transactions = getRpcTransactions(blockHash, blockCount, transactionFilter);
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting transactions: " << x.what();
    return x.code();
  } catch (std::exception& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting transactions: " << x.what();
    return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
  }

  return std::error_code();
}

std::error_code WalletService::getTransactions(const std::vector<std::string>& addresses, uint32_t firstBlockIndex,
  uint32_t blockCount, const std::string& paymentId, std::vector<TransactionsInBlockRpcInfo>& transactions) {
  try {
    System::EventLock lk(readyEvent);
    validateAddresses(addresses, currency, logger);

    if (!paymentId.empty()) {
      validatePaymentId(paymentId, logger);
    }

    TransactionsInBlockInfoFilter transactionFilter(addresses, paymentId);

    transactions = getRpcTransactions(firstBlockIndex, blockCount, transactionFilter);
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting transactions: " << x.what();
    return x.code();
  } catch (std::exception& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting transactions: " << x.what();
    return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
  }

  return std::error_code();
}

std::error_code WalletService::getTransaction(const std::string& transactionHash, TransactionRpcInfo& transaction) {
  try {
    System::EventLock lk(readyEvent);
    Crypto::Hash hash = parseHash(transactionHash, logger);

    CryptoNote::WalletTransactionWithTransfers transactionWithTransfers = wallet.getTransaction(hash);

    if (transactionWithTransfers.transaction.state == CryptoNote::WalletTransactionState::DELETED) {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Transaction " << transactionHash << " is deleted";
      return make_error_code(CryptoNote::error::OBJECT_NOT_FOUND);
    }

    transaction = convertTransactionWithTransfersToTransactionRpcInfo(transactionWithTransfers);
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting transaction: " << x.what();
    return x.code();
  } catch (std::exception& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting transaction: " << x.what();
    return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
  }

  return std::error_code();
}

std::error_code WalletService::getAddresses(std::vector<std::string>& addresses) {
  try {
    System::EventLock lk(readyEvent);

    addresses.clear();
    addresses.reserve(wallet.getAddressCount());

    for (size_t i = 0; i < wallet.getAddressCount(); ++i) {
      addresses.push_back(wallet.getAddress(i));
    }
  } catch (std::exception& e) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Can't get addresses: " << e.what();
    return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
  }

  return std::error_code();
}

std::error_code WalletService::sendTransaction(SendTransaction::Request& request, std::string& transactionHash) {
  try {
    System::EventLock lk(readyEvent);

    /* Integrated address payment ID's are uppercase - lets convert the input
       payment ID to upper so we can compare with more ease */
    std::transform(request.paymentId.begin(), request.paymentId.end(), request.paymentId.begin(), ::toupper);

    std::vector<std::string> paymentIDs;

    for (auto &transfer : request.transfers)
    {
        std::string addr = transfer.address;

        /* It's not a standard address. Is it an integrated address? */
        if (!CryptoNote::validateAddress(addr, currency))
        {
            std::string address, paymentID;
            std::tie(address, paymentID) = decodeIntegratedAddress(addr, currency, logger);
            
            /* A payment ID was specified with the transaction, and it is not
               the same as the decoded one -> we can't send a transaction
               with two different payment ID's! */
            if (request.paymentId != "" && request.paymentId != paymentID)
            {
                throw std::system_error(make_error_code(CryptoNote::error::CONFLICTING_PAYMENT_IDS));
            }
            
            /* Replace the integrated transfer address with the actual
               decoded address */
            transfer.address = address;

            paymentIDs.push_back(paymentID);
        }
    }

    /* Only one integrated address specified, set the payment ID to the
       decoded value */
    if (paymentIDs.size() == 1)
    {
        request.paymentId = paymentIDs[0];
        
    }
    else if (paymentIDs.size() > 1)
    {   
        /* Are all the specified payment IDs equal? */
        if (!std::equal(paymentIDs.begin() + 1, paymentIDs.end(), paymentIDs.begin()))
        {
            throw std::system_error(make_error_code(CryptoNote::error::CONFLICTING_PAYMENT_IDS));
        }

        /* They are all equal, set the payment ID to the decoded value */
        request.paymentId = paymentIDs[0];
    }

    validateAddresses(request.sourceAddresses, currency, logger);
    validateAddresses(collectDestinationAddresses(request.transfers), currency, logger);
    if (!request.changeAddress.empty()) {
      validateAddresses({ request.changeAddress }, currency, logger);
    }

    auto [success, error, error_code] = Utilities::validate(request.anonymity, node.getLastKnownBlockHeight());

    if (!success)
    {
      logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << error;
      throw std::system_error(error_code);
    }

    CryptoNote::TransactionParameters sendParams;
    if (!request.paymentId.empty()) {
      addPaymentIdToExtra(request.paymentId, sendParams.extra);
    } else {
      sendParams.extra = getValidatedTransactionExtraString(request.extra);
    }

    sendParams.sourceAddresses = request.sourceAddresses;
    sendParams.destinations = convertWalletRpcOrdersToWalletOrders(request.transfers, m_node_address, m_node_fee);
    sendParams.fee = request.fee;
    sendParams.mixIn = request.anonymity;
    sendParams.unlockTimestamp = request.unlockTime;
    sendParams.changeDestination = request.changeAddress;

    size_t transactionId = wallet.transfer(sendParams);
    transactionHash = Common::podToHex(wallet.getTransaction(transactionId).hash);

    logger(Logging::DEBUGGING) << "Transaction " << transactionHash << " has been sent";
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while sending transaction: " << x.what();
    return x.code();
  } catch (std::exception& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while sending transaction: " << x.what();
    return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
  }

  return std::error_code();
}

std::error_code WalletService::createDelayedTransaction(CreateDelayedTransaction::Request& request, std::string& transactionHash) {
  try {
    System::EventLock lk(readyEvent);

    /* Integrated address payment ID's are uppercase - lets convert the input
       payment ID to upper so we can compare with more ease */
    std::transform(request.paymentId.begin(), request.paymentId.end(), request.paymentId.begin(), ::toupper);

    std::vector<std::string> paymentIDs;

    for (auto &transfer : request.transfers)
    {
        std::string addr = transfer.address;

        /* It's not a standard address. Is it an integrated address? */
        if (!CryptoNote::validateAddress(addr, currency))
        {
            std::string address, paymentID; 
            std::tie(address, paymentID) = decodeIntegratedAddress(addr, currency, logger);
            
            /* A payment ID was specified with the transaction, and it is not
               the same as the decoded one -> we can't send a transaction
               with two different payment ID's! */
            if (request.paymentId != "" && request.paymentId != paymentID)
            {
                throw std::system_error(make_error_code(CryptoNote::error::CONFLICTING_PAYMENT_IDS));
            }
            
            /* Replace the integrated transfer address with the actual
               decoded address */
            transfer.address = address;

            paymentIDs.push_back(paymentID);
        }
    }

    /* Only one integrated address specified, set the payment ID to the
       decoded value */
    if (paymentIDs.size() == 1)
    {
        request.paymentId = paymentIDs[0];
    }
    else if (paymentIDs.size() > 1)
    {
        /* Are all the specified payment IDs equal? */
        if (!std::equal(paymentIDs.begin() + 1, paymentIDs.end(), paymentIDs.begin()))
        {
            throw std::system_error(make_error_code(CryptoNote::error::CONFLICTING_PAYMENT_IDS));
        }

        /* They are all equal, set the payment ID to the decoded value */
        request.paymentId = paymentIDs[0];
    }


    validateAddresses(request.addresses, currency, logger);
    validateAddresses(collectDestinationAddresses(request.transfers), currency, logger);
    if (!request.changeAddress.empty()) {
      validateAddresses({ request.changeAddress }, currency, logger);
    }

    CryptoNote::TransactionParameters sendParams;
    if (!request.paymentId.empty()) {
      addPaymentIdToExtra(request.paymentId, sendParams.extra);
    } else {
      sendParams.extra = Common::asString(Common::fromHex(request.extra));
    }

    sendParams.sourceAddresses = request.addresses;
    sendParams.destinations = convertWalletRpcOrdersToWalletOrders(request.transfers, m_node_address, m_node_fee);
    sendParams.fee = request.fee;
    sendParams.mixIn = request.anonymity;
    sendParams.unlockTimestamp = request.unlockTime;
    sendParams.changeDestination = request.changeAddress;

    size_t transactionId = wallet.makeTransaction(sendParams);
    transactionHash = Common::podToHex(wallet.getTransaction(transactionId).hash);

    logger(Logging::DEBUGGING) << "Delayed transaction " << transactionHash << " has been created";
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while creating delayed transaction: " << x.what();
    return x.code();
  } catch (std::exception& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while creating delayed transaction: " << x.what();
    return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
  }

  return std::error_code();
}

std::error_code WalletService::getDelayedTransactionHashes(std::vector<std::string>& transactionHashes) {
  try {
    System::EventLock lk(readyEvent);

    std::vector<size_t> transactionIds = wallet.getDelayedTransactionIds();
    transactionHashes.reserve(transactionIds.size());

    for (auto id: transactionIds) {
      transactionHashes.emplace_back(Common::podToHex(wallet.getTransaction(id).hash));
    }

  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting delayed transaction hashes: " << x.what();
    return x.code();
  } catch (std::exception& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting delayed transaction hashes: " << x.what();
    return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
  }

  return std::error_code();
}

std::error_code WalletService::deleteDelayedTransaction(const std::string& transactionHash) {
  try {
    System::EventLock lk(readyEvent);

    parseHash(transactionHash, logger); //validate transactionHash parameter

    auto idIt = transactionIdIndex.find(transactionHash);
    if (idIt == transactionIdIndex.end()) {
      return make_error_code(CryptoNote::error::WalletServiceErrorCode::OBJECT_NOT_FOUND);
    }

    size_t transactionId = idIt->second;
    wallet.rollbackUncommitedTransaction(transactionId);

    logger(Logging::DEBUGGING) << "Delayed transaction " << transactionHash << " has been canceled";
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while deleting delayed transaction hashes: " << x.what();
    return x.code();
  } catch (std::exception& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while deleting delayed transaction hashes: " << x.what();
    return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
  }

  return std::error_code();
}

std::error_code WalletService::sendDelayedTransaction(const std::string& transactionHash) {
  try {
    System::EventLock lk(readyEvent);

    parseHash(transactionHash, logger); //validate transactionHash parameter

    auto idIt = transactionIdIndex.find(transactionHash);
    if (idIt == transactionIdIndex.end()) {
      return make_error_code(CryptoNote::error::WalletServiceErrorCode::OBJECT_NOT_FOUND);
    }

    size_t transactionId = idIt->second;
    wallet.commitTransaction(transactionId);

    logger(Logging::DEBUGGING) << "Delayed transaction " << transactionHash << " has been sent";
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while sending delayed transaction hashes: " << x.what();
    return x.code();
  } catch (std::exception& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while sending delayed transaction hashes: " << x.what();
    return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
  }

  return std::error_code();
}

std::error_code WalletService::getUnconfirmedTransactionHashes(const std::vector<std::string>& addresses, std::vector<std::string>& transactionHashes) {
  try {
    System::EventLock lk(readyEvent);

    validateAddresses(addresses, currency, logger);

    std::vector<CryptoNote::WalletTransactionWithTransfers> transactions = wallet.getUnconfirmedTransactions();

    TransactionsInBlockInfoFilter transactionFilter(addresses, "");

    for (const auto& transaction: transactions) {
      if (transactionFilter.checkTransaction(transaction)) {
        transactionHashes.emplace_back(Common::podToHex(transaction.transaction.hash));
      }
    }
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting unconfirmed transaction hashes: " << x.what();
    return x.code();
  } catch (std::exception& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting unconfirmed transaction hashes: " << x.what();
    return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
  }

  return std::error_code();
}

/* blockCount = the blocks the wallet has synced. knownBlockCount = the top block the daemon knows of. localDaemonBlockCount = the blocks the daemon has synced. */
std::error_code WalletService::getStatus(uint32_t& blockCount, uint32_t& knownBlockCount, uint64_t& localDaemonBlockCount, std::string& lastBlockHash, uint32_t& peerCount) {
  try {
    System::EventLock lk(readyEvent);

    System::RemoteContext<std::tuple<uint32_t, uint64_t, uint32_t>> remoteContext(dispatcher, [this] () {
      /* Daemon remote height, daemon local height, peer count */
      return std::make_tuple(node.getKnownBlockCount(), node.getNodeHeight(), static_cast<uint32_t>(node.getPeerCount()));
    });

    std::tie(knownBlockCount, localDaemonBlockCount, peerCount) = remoteContext.get();

    blockCount = wallet.getBlockCount();

    auto lastHashes = wallet.getBlockHashes(blockCount - 1, 1);
    lastBlockHash = Common::podToHex(lastHashes.back());
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting status: " << x.what();
    return x.code();
  } catch (std::exception& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while getting status: " << x.what();
    return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
  }

  return std::error_code();
}

std::error_code WalletService::sendFusionTransaction(uint64_t threshold, uint32_t anonymity, const std::vector<std::string>& addresses,
  const std::string& destinationAddress, std::string& transactionHash) {

  try {
    System::EventLock lk(readyEvent);

    validateAddresses(addresses, currency, logger);
    if (!destinationAddress.empty()) {
      validateAddresses({ destinationAddress }, currency, logger);
    }

    size_t transactionId = fusionManager.createFusionTransaction(threshold, anonymity, addresses, destinationAddress);
    transactionHash = Common::podToHex(wallet.getTransaction(transactionId).hash);

    logger(Logging::DEBUGGING) << "Fusion transaction " << transactionHash << " has been sent";
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while sending fusion transaction: " << x.what();
    return x.code();
  } catch (std::exception& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while sending fusion transaction: " << x.what();
    return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
  }

  return std::error_code();
}

std::error_code WalletService::estimateFusion(uint64_t threshold, const std::vector<std::string>& addresses,
  uint32_t& fusionReadyCount, uint32_t& totalOutputCount) {

  try {
    System::EventLock lk(readyEvent);

    validateAddresses(addresses, currency, logger);

    auto estimateResult = fusionManager.estimate(threshold, addresses);
    fusionReadyCount = static_cast<uint32_t>(estimateResult.fusionReadyCount);
    totalOutputCount = static_cast<uint32_t>(estimateResult.totalOutputCount);
  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Failed to estimate number of fusion outputs: " << x.what();
    return x.code();
  } catch (std::exception& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Failed to estimate number of fusion outputs: " << x.what();
    return make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR);
  }

  return std::error_code();
}

std::error_code WalletService::createIntegratedAddress(const std::string &address, const std::string &paymentId, std::string& integratedAddress) {
  try {
    System::EventLock lk(readyEvent);

    validateAddresses({address}, currency, logger);
    validatePaymentId(paymentId, logger);

  } catch (std::system_error& x) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "Error while creating integrated address: " << x.what();
    return x.code();
  }

  uint64_t prefix;

  CryptoNote::AccountPublicAddress addr;

  /* Get the private + public key from the address */
  Utilities::parseAccountAddressString(prefix, addr, address);

  /* Pack as a binary array */
  CryptoNote::BinaryArray ba;
  CryptoNote::toBinaryArray(addr, ba);
  std::string keys = Common::asString(ba);

  /* Encode prefix + paymentID + keys as an address */
  integratedAddress = Tools::Base58::encode_addr
  (
      CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
      paymentId + keys
  );

  return std::error_code();
}

std::error_code WalletService::getFeeInfo(std::string& address, uint32_t& amount) {
  address = m_node_address;
  amount = m_node_fee;

  return std::error_code();
}

uint64_t WalletService::getDefaultMixin() const
{
    return CryptoNote::getDefaultMixinByHeight(node.getLastKnownBlockHeight());
}

void WalletService::refresh() {
  try {
    logger(Logging::DEBUGGING) << "Refresh is started";
    for (;;) {
      auto event = wallet.getEvent();
      if (event.type == CryptoNote::TRANSACTION_CREATED) {
        size_t transactionId = event.transactionCreated.transactionIndex;
        transactionIdIndex.emplace(Common::podToHex(wallet.getTransaction(transactionId).hash), transactionId);
      }
    }
  } catch (std::system_error& e) {
    logger(Logging::DEBUGGING) << "refresh is stopped: " << e.what();
  } catch (std::exception& e) {
    logger(Logging::WARNING, Logging::BRIGHT_YELLOW) << "exception thrown in refresh(): " << e.what();
  }
}

void WalletService::reset(const uint64_t scanHeight) {
  wallet.reset(scanHeight);
}

std::vector<CryptoNote::TransactionsInBlockInfo> WalletService::getTransactions(const Crypto::Hash& blockHash, size_t blockCount) const {
  std::vector<CryptoNote::TransactionsInBlockInfo> result = wallet.getTransactions(blockHash, blockCount);
  if (result.empty()) {
    throw std::system_error(make_error_code(CryptoNote::error::WalletServiceErrorCode::OBJECT_NOT_FOUND));
  }

  return result;
}

std::vector<CryptoNote::TransactionsInBlockInfo> WalletService::getTransactions(uint32_t firstBlockIndex, size_t blockCount) const {
  std::vector<CryptoNote::TransactionsInBlockInfo> result = wallet.getTransactions(firstBlockIndex, blockCount);
  if (result.empty()) {
    throw std::system_error(make_error_code(CryptoNote::error::WalletServiceErrorCode::OBJECT_NOT_FOUND));
  }

  return result;
}

std::vector<TransactionHashesInBlockRpcInfo> WalletService::getRpcTransactionHashes(const Crypto::Hash& blockHash, size_t blockCount, const TransactionsInBlockInfoFilter& filter) const {
  std::vector<CryptoNote::TransactionsInBlockInfo> allTransactions = getTransactions(blockHash, blockCount);
  std::vector<CryptoNote::TransactionsInBlockInfo> filteredTransactions = filterTransactions(allTransactions, filter);
  return convertTransactionsInBlockInfoToTransactionHashesInBlockRpcInfo(filteredTransactions);
}

std::vector<TransactionHashesInBlockRpcInfo> WalletService::getRpcTransactionHashes(uint32_t firstBlockIndex, size_t blockCount, const TransactionsInBlockInfoFilter& filter) const {
  std::vector<CryptoNote::TransactionsInBlockInfo> allTransactions = getTransactions(firstBlockIndex, blockCount);
  std::vector<CryptoNote::TransactionsInBlockInfo> filteredTransactions = filterTransactions(allTransactions, filter);
  return convertTransactionsInBlockInfoToTransactionHashesInBlockRpcInfo(filteredTransactions);
}

std::vector<TransactionsInBlockRpcInfo> WalletService::getRpcTransactions(const Crypto::Hash& blockHash, size_t blockCount, const TransactionsInBlockInfoFilter& filter) const {
  std::vector<CryptoNote::TransactionsInBlockInfo> allTransactions = getTransactions(blockHash, blockCount);
  std::vector<CryptoNote::TransactionsInBlockInfo> filteredTransactions = filterTransactions(allTransactions, filter);
  return convertTransactionsInBlockInfoToTransactionsInBlockRpcInfo(filteredTransactions);
}

std::vector<TransactionsInBlockRpcInfo> WalletService::getRpcTransactions(uint32_t firstBlockIndex, size_t blockCount, const TransactionsInBlockInfoFilter& filter) const {
  std::vector<CryptoNote::TransactionsInBlockInfo> allTransactions = getTransactions(firstBlockIndex, blockCount);
  std::vector<CryptoNote::TransactionsInBlockInfo> filteredTransactions = filterTransactions(allTransactions, filter);
  return convertTransactionsInBlockInfoToTransactionsInBlockRpcInfo(filteredTransactions);
}

} //namespace PaymentService
