// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoTypes.h"

#include "IWallet.h"

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

/* CryptoNote::WalletTransaction */
void to_json(json &j, const CryptoNote::WalletTransaction &t);
void from_json(const json &j, CryptoNote::WalletTransaction &t);

/* Crypto::Hash */
void to_json(json &j, const Crypto::Hash &h);
void from_json(const json &j, Crypto::Hash &h);

/* Generic serializers for any hash type with a data member
   (e.g., CryptoTypes.h) */
template<typename Data>
void hashToJson(json &j, const Data &d, const std::string &dataName)
{
    j = json {
        {dataName, d.data}
    };
}

template<typename Data>
void jsonToHash(const json &j, Data &d, const std::string &dataName)
{
    auto data = j.at(dataName).get<std::array<uint8_t, sizeof(Data::data)>>();

    for (size_t i = 0; i < data.size(); i++)
    {
        d.data[i] = data[i];
    }
}
