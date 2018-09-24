// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <crypto/crypto.h>

#include "CryptoTypes.h"

/* TODO: Kill IWallet.h */
#include "IWallet.h"

#include "json.hpp"

#include <string>

#include <unordered_set>

#include <WalletBackend/WalletErrors.h>

using nlohmann::json;

class SubWallet
{
    public:
        SubWallet();

        SubWallet(const Crypto::PublicKey publicSpendKey,
                  const std::string address,
                  const uint64_t scanHeight, const bool newWallet);

        SubWallet(const Crypto::PublicKey publicSpendKey,
                  const Crypto::SecretKey privateSpendKey,
                  const std::string address,
                  const uint64_t scanHeight, const bool newWallet);

        /* Converts the class to a json object */
        json toJson() const;

        /* Initializes the class from a json string */
        void fromJson(const json &j);

        void generateAndStoreKeyImage(Crypto::KeyDerivation derivation,
                                      size_t outputIndex);

        void addTransfer(int64_t amount);

        /* The timestamp to begin syncing the wallet at
           (usually creation time) */
        uint64_t m_syncStartTimestamp = 0;

        /* Whether this is a view only wallet */
        bool m_isViewWallet;

        /* A set of the stored key images we own (Key images are unique) */
        std::unordered_set<Crypto::KeyImage> m_keyImages;

        /* This subwallet's public spend key */
        Crypto::PublicKey m_publicSpendKey;

    private:
        /* The subwallet's private spend key */
        Crypto::SecretKey m_privateSpendKey;

        /* This subwallet's public address */
        std::string m_address;

        /* TODO: Trim down WalletTransaction to be a tad more concise */
        std::vector<CryptoNote::WalletTransaction> m_transactions;
};
