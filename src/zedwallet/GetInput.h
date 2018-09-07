// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <string>

#include <vector>

#include <zedwallet/Commands.h>
#include <zedwallet/Types.h>

std::string yellowANSIMsg(std::string msg);

std::string getPrompt(std::shared_ptr<WalletInfo> walletInfo);

template<typename T>
std::string getInputAndWorkInBackground(const std::vector<T>
                                        &availableCommands,
                                        std::string prompt,
                                        bool backgroundRefresh,
                                        std::shared_ptr<WalletInfo>
                                        walletInfo);

template<typename T>
std::string getInput(const std::vector<T> &availableCommands,
                     std::string prompt);
