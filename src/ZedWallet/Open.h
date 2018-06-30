/*
Copyright (C) 2018, The TurtleCoin developers

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <ZedWallet/Types.h>

std::shared_ptr<WalletInfo> importFromKeys(CryptoNote::WalletGreen &wallet,
                                           Crypto::SecretKey privateSpendKey,
                                           Crypto::SecretKey privateViewKey);

Maybe<std::shared_ptr<WalletInfo>> openWallet(CryptoNote::WalletGreen &wallet,
                                              Config &config);

std::shared_ptr<WalletInfo> importWallet(CryptoNote::WalletGreen &wallet);

std::shared_ptr<WalletInfo> createViewWallet(CryptoNote::WalletGreen &wallet);

std::shared_ptr<WalletInfo> mnemonicImportWallet(CryptoNote::WalletGreen
                                                 &wallet);

std::shared_ptr<WalletInfo> generateWallet(CryptoNote::WalletGreen &wallet);

Crypto::SecretKey getPrivateKey(std::string outputMsg);

std::string getNewWalletFileName();

std::string getExistingWalletFileName(Config &config);

std::string getWalletPassword(bool verifyPwd);

bool isValidMnemonic(std::string &mnemonic_phrase,
                     Crypto::SecretKey &private_spend_key);

void logIncorrectMnemonicWords(std::vector<std::string> words);

void viewWalletMsg();

void connectingMsg();

void promptSaveKeys(CryptoNote::WalletGreen &wallet);
