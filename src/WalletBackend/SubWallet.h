// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <crypto/crypto.h>

#include "CryptoTypes.h"

#include "json.hpp"

#include <string>

#include <unordered_set>

#include <WalletBackend/Transaction.h>
#include <WalletBackend/WalletErrors.h>

#include "WalletTypes.h"

using nlohmann::json;

class SubWallet
{
    public:
        SubWallet();

        SubWallet(const Crypto::PublicKey publicSpendKey,
                  const std::string address,
                  const uint64_t scanHeight, const uint64_t scanTimestamp);

        SubWallet(const Crypto::PublicKey publicSpendKey,
                  const Crypto::SecretKey privateSpendKey,
                  const std::string address,
                  const uint64_t scanHeight, const uint64_t scanTimestamp);

        /* Converts the class to a json object */
        json toJson() const;

        /* Initializes the class from a json string */
        void fromJson(const json &j);

        /* Generates a key image from the derivation, and stores it */
        void generateAndStoreKeyImage(Crypto::KeyDerivation derivation,
                                      size_t outputIndex, uint64_t amount);

        /* Whether this is a view only wallet */
        bool m_isViewWallet;

        /* A vector of the stored key images we own (Key images are unique) and
           their amounts */
        std::vector<WalletTypes::TransactionInput> m_keyImages;

        /* This subwallet's public spend key */
        Crypto::PublicKey m_publicSpendKey;

        /* This wallets balance */
        uint64_t m_balance = 0;

        /* The timestamp to begin syncing the wallet at
           (usually creation time or zero) */
        uint64_t m_syncStartTimestamp = 0;

        /* The height to begin syncing the wallet at */
        uint64_t m_syncStartHeight = 0;

    private:
        /* The subwallet's private spend key */
        Crypto::SecretKey m_privateSpendKey;

        /* This subwallet's public address */
        std::string m_address;
};
