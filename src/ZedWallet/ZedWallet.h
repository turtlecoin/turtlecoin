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

#include <ZedWallet/ColouredMsg.h>
#include <ZedWallet/Types.h>

enum Action
{
    Open, Generate, Import, SeedImport, ViewWallet
};

Action getAction(Config &config);

void welcomeMsg();

void run(CryptoNote::WalletGreen &wallet, CryptoNote::INode &node,
         Config &config);

void inputLoop(std::shared_ptr<WalletInfo> &walletInfo,
               CryptoNote::INode &node);

bool shutdown(CryptoNote::WalletGreen &wallet, CryptoNote::INode &node,
              bool &alreadyShuttingDown);

std::string getInputAndDoWorkWhileIdle(std::shared_ptr<WalletInfo> &walletInfo);


Maybe<std::shared_ptr<WalletInfo>> handleAction(CryptoNote::WalletGreen &wallet,
                                                Action action, Config &config);
