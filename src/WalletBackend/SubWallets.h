// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <crypto/crypto.h>

#include <WalletBackend/SubWallet.h>

class SubWallets
{
    public:
        SubWallets();

        SubWallets(const Crypto::PublicKey publicSpendKey,
                   const std::string address, const uint64_t scanHeight,
                   const bool newWallet);

        SubWallets(const Crypto::SecretKey privateSpendKey,
                   const std::string address,
                   const uint64_t scanHeight, const bool newWallet);

        void addSubWallet(const Crypto::PublicKey publicSpendKey,
                          const std::string address,
                          const uint64_t scanHeight, const bool newWallet);

        void addSubWallet(const Crypto::SecretKey privateSpendKey,
                          const std::string address,
                          const uint64_t scanHeight, const bool newWallet);

        /* The public spend keys, used for verifying if a transaction is
           ours */
        std::vector<Crypto::PublicKey> m_publicSpendKeys;

        uint64_t getMinSyncTimestamp();

        /* Converts the class to a json object */
        json toJson() const;

        /* Initializes the class from a json string */
        void fromJson(const json &j);

        void addTransfers(std::unordered_map<Crypto::PublicKey, int64_t>
                          transfers);

        /* Generates a key image using the public+private spend key of the
           subwallet. Wallet must not be a view wallet (and must exist, but
           the WalletSynchronizer already checks this) */
        void generateAndStoreKeyImage(Crypto::PublicKey, Crypto::KeyDerivation,
                                      size_t outputIndex);

        std::tuple<bool, Crypto::PublicKey>
            getKeyImageOwner(Crypto::KeyImage keyImage);

    private:
        /* The subwallets, indexed by public spend key */ 
        std::unordered_map<Crypto::PublicKey, SubWallet> m_subWallets;
};
