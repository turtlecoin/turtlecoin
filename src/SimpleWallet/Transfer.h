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

#include "CryptoNoteConfig.h"
#include "IWallet.h"

#include <Common/StringTools.h>

#include <CryptoNoteCore/CryptoNoteBasicImpl.h>

#include <SimpleWallet/Fusion.h>

#include <Wallet/WalletGreen.h>

#include <boost/algorithm/string.hpp>

struct WalletInfo
{
    WalletInfo(std::string walletFileName, 
               std::string walletPass, 
               std::string walletAddress,
               bool viewWallet,
               CryptoNote::WalletGreen &wallet) : 
               walletFileName(walletFileName), 
               walletPass(walletPass), 
               walletAddress(walletAddress),
               viewWallet(viewWallet),
               wallet(wallet) {}

    size_t knownTransactionCount = 0;

    std::string walletFileName;
    std::string walletPass;
    std::string walletAddress;

    bool viewWallet;

    CryptoNote::WalletGreen &wallet;
};

void transfer(std::shared_ptr<WalletInfo> walletInfo);

void transfer(std::shared_ptr<WalletInfo> walletInfo,
              std::vector<std::string> args);

void doTransfer(uint16_t mixin, std::string address, uint64_t amount,
                uint64_t fee, std::string extra,
                std::shared_ptr<WalletInfo> walletInfo);

void sendMultipleTransactions(CryptoNote::WalletGreen &wallet,
                              std::vector<CryptoNote::TransactionParameters>
                              transfers);

void splitTx(CryptoNote::WalletGreen &wallet,
             CryptoNote::TransactionParameters p);

bool confirmTransaction(CryptoNote::TransactionParameters t,
                        std::shared_ptr<WalletInfo> walletInfo);

bool parseAmount(std::string amountString);

bool parseMixin(std::string mixinString);

bool parseAddress(std::string address);

bool parseFee(std::string feeString);

Maybe<std::string> getPaymentID();

Maybe<std::string> getDestinationAddress();

Maybe<uint64_t> getFee();

Maybe<uint64_t> getTransferAmount();

Maybe<uint16_t> getMixin();
