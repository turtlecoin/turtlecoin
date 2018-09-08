// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <string>

#include <unordered_map>

#include <vector>

#include <WalletBackend/WalletErrors.h>

WalletError validateTransaction(
    std::unordered_map<std::string, uint64_t> destinations,
    uint64_t mixin, uint64_t fee, std::string paymentID,
    std::vector<std::string> subWalletsToTakeFrom, std::string changeAddress);

WalletError validateAddresses(std::vector<std::string> addresses);
