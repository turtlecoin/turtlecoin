// Copyright (c) 2018-2019, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <WalletBackend/WalletBackend.h>

void transfer(
        const std::shared_ptr<WalletBackend> walletBackend,
        const bool sendAll);

void sendTransaction(
        const std::shared_ptr<WalletBackend> walletBackend,
        const std::string address,
        const uint64_t amount,
        const std::string paymentID);

void splitTX(
        const std::shared_ptr<WalletBackend> walletBackend,
        const std::string address,
        const uint64_t amount,
        const std::string paymentID);

bool confirmTransaction(
        const std::shared_ptr<WalletBackend> walletBackend,
        const std::string address,
        const uint64_t amount,
        const std::string paymentID,
        const uint64_t nodeFee);
