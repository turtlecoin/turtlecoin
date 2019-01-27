// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <Common/StringTools.h>

#include "CryptoTypes.h"

#include "IWallet.h"

#include "json.hpp"

#include <SubWallets/SubWallet.h>

#include <WalletBackend/WalletBackend.h>

using nlohmann::json;

/* Tmp struct just used in serialization (See cpp for justification) */
struct Transfer
{
    Crypto::PublicKey publicKey;
    int64_t amount;
};

/* As above */
struct TxPrivateKey
{
    Crypto::Hash txHash;
    Crypto::SecretKey txPrivateKey;
};

/* Transfer */
void to_json(json &j, const Transfer &t);
void from_json(const json &j, Transfer &t);

void to_json(json &j, const TxPrivateKey &t);
void from_json(const json &j, TxPrivateKey &t);

namespace WalletTypes
{
    /* WalletTypes::Transaction */
    void to_json(json &j, const WalletTypes::Transaction &t);
    void from_json(const json &j, WalletTypes::Transaction &t);
}

std::vector<Transfer> transfersToVector(
    const std::unordered_map<Crypto::PublicKey, int64_t> transfers);

std::unordered_map<Crypto::PublicKey, int64_t> vectorToTransfers(
    const std::vector<Transfer> vector);

std::vector<TxPrivateKey> txPrivateKeysToVector(
    const std::unordered_map<Crypto::Hash, Crypto::SecretKey> txPrivateKeys);

std::unordered_map<Crypto::Hash, Crypto::SecretKey> vectorToTxPrivateKeys(
    const std::vector<TxPrivateKey> vector);
