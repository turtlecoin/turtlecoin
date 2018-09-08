// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoNote.h>

#include <string>

#include <vector>

#include <WalletBackend/WalletErrors.h>

std::vector<Crypto::PublicKey> addressesToViewKeys(std::vector<std::string> addresses);
