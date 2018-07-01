// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#include <string>

#include <vector>

#include <zedwallet/Types.h>

#pragma once

const Maybe<Command> contains(std::string name,
                              std::vector<Command> &commands);

const std::vector<Command> filterCommands(std::vector<Command> &commands,
                                          std::function<bool(Command)> predicate);

std::vector<Command> allCommands();

const std::vector<Command> availableCommands(bool viewWallet,
                                             std::vector<Command> &commands);

void listCommands(std::vector<Command> &commands, bool advanced);

int numBasicCommands(std::vector<Command> &commands);

const Maybe<Command> resolveCommand(std::string command,
                                    std::vector<Command> &allCommands,
                                    std::vector<Command> &available);

bool dispatchCommand(std::shared_ptr<WalletInfo> &walletInfo,
                     CryptoNote::INode &node, std::string command);
