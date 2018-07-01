// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <zedwallet/Types.h>

#include <Wallet/WalletGreen.h>

void changePassword(std::shared_ptr<WalletInfo> &walletInfo);

void printPrivateKeys(CryptoNote::WalletGreen &wallet, bool viewWallet);

void status(CryptoNote::INode &node);

void reset(CryptoNote::INode &node, std::shared_ptr<WalletInfo> &walletInfo);

void blockchainHeight(CryptoNote::INode &node, CryptoNote::WalletGreen &wallet);

void balance(CryptoNote::INode &node, CryptoNote::WalletGreen &wallet,
             bool viewWallet);

void exportKeys(std::shared_ptr<WalletInfo> &walletInfo);

void saveCSV(CryptoNote::WalletGreen &wallet, CryptoNote::INode &node);

void save(CryptoNote::WalletGreen &wallet);

void listTransfers(bool incoming, bool outgoing,
                   CryptoNote::WalletGreen &wallet, CryptoNote::INode &node);

void printOutgoingTransfer(CryptoNote::WalletTransaction t,
                           CryptoNote::INode &node,
                           bool fetchTimestamps);

void printIncomingTransfer(CryptoNote::WalletTransaction t,
                           CryptoNote::INode &node,
                           bool fetchTimestamps);

std::string getBlockTimestamp(CryptoNote::BlockDetails b);

CryptoNote::BlockDetails getBlock(uint32_t blockHeight,
                                  CryptoNote::INode &node);
