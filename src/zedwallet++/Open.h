// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <walletbackend/WalletBackend.h>
#include <zedwallet++/ParseArguments.h>

std::shared_ptr<WalletBackend> openWallet(const ZedConfig &config);

std::shared_ptr<WalletBackend> importViewWallet(const ZedConfig &config);

std::shared_ptr<WalletBackend> importWalletFromKeys(const ZedConfig &config);

std::shared_ptr<WalletBackend> importWalletFromSeed(const ZedConfig &config);

std::shared_ptr<WalletBackend> createWallet(const ZedConfig &config);

Crypto::SecretKey getPrivateKey(const std::string outputMsg);

std::string getNewWalletFileName();

std::string getExistingWalletFileName(const ZedConfig &config);

std::string getWalletPassword(const bool verifyPwd, const std::string msg);

void viewWalletMsg();

void promptSaveKeys(const std::shared_ptr<WalletBackend> walletBackend);
