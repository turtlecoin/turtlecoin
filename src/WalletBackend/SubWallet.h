// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoTypes.h"

#include "json.hpp"

#include <string>

#include <WalletBackend/WalletErrors.h>

using nlohmann::json;

class SubWallet
{
    public:
        SubWallet() {};

        SubWallet(Crypto::SecretKey privateSpendKey,
                  std::string address,
                  uint64_t scanHeight, bool newWallet);

        /* Converts the class to a json object */
        json toJson() const;

        /* Initializes the class from a json string */
        void fromJson(const json &j);

    private:
        /* This subwallet's private spend key */
        Crypto::SecretKey m_privateSpendKey;

        /* This subwallet's public address */
        std::string m_address;

        /* The height to begin scanning for transactions at in this wallet */
        uint64_t m_scanHeight = 0;

        /* The timestamp this wallet was created at */
        uint64_t m_creationTimestamp = 0;
};
