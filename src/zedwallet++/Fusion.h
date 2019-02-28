// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <WalletBackend/WalletBackend.h>

void optimize(const std::shared_ptr<WalletBackend> walletBackend);

bool optimizeRound(const std::shared_ptr<WalletBackend> walletBackend);
