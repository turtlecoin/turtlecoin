// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoTypes.h"

/* TODO: Kill IWallet.h */
#include "IWallet.h"

#include "json.hpp"

#include <string>

#include <WalletBackend/WalletErrors.h>

using nlohmann::json;

class SubWallet
{
    public:
        SubWallet();

        SubWallet(Crypto::SecretKey privateSpendKey,
                  std::string address,
                  uint64_t scanHeight, bool newWallet);

        /* Converts the class to a json object */
        json toJson() const;

        /* Initializes the class from a json string */
        void fromJson(const json &j);

        /* The timestamp to begin syncing the wallet at
           (usually creation time) */
        uint64_t m_syncStartTimestamp = 0;

    private:
        /* This subwallet's private spend key */
        Crypto::SecretKey m_privateSpendKey;

        /* This subwallet's public address */
        std::string m_address;

        /* TODO: Trim down WalletTransaction to be a tad more concise */
        std::vector<CryptoNote::WalletTransaction> m_transactions;
};
