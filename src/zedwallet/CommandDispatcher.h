// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2019, The BytechCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <zedwallet/Types.h>

bool handleCommand(const std::string command,
                   std::shared_ptr<WalletInfo> walletInfo,
                   CryptoNote::INode &node);

std::shared_ptr<WalletInfo> handleLaunchCommand(CryptoNote::WalletGreen &wallet,
                                                std::string launchCommand,
                                                Config &config);
