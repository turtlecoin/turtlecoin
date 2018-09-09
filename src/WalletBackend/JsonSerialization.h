// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoTypes.h"

#include "json.hpp"

#include <WalletBackend/SubWallet.h>
#include <WalletBackend/WalletBackend.h>

using nlohmann::json;

/* SubWallet */
void to_json(json &j, const SubWallet &s);
void from_json(const json &j, SubWallet &s);

/* WalletBackend */
void to_json(json &j, const WalletBackend &w);
void from_json(const json &j, WalletBackend &w);

/* Crypto::SecretKey */
void to_json(json &j, const Crypto::SecretKey &s);
void from_json(const json &j, Crypto::SecretKey &s);
