// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <vector>
#include <ostream>
#include <istream>

#include "crypto/hash.h"
#include "crypto/chacha8.h"

namespace CryptoNote
{
    class AccountBase;

    class ISerializer;
}

namespace CryptoNote
{

    class WalletUserTransactionsCache;

    class WalletLegacySerializer
    {
    public:
        WalletLegacySerializer(CryptoNote::AccountBase &account, WalletUserTransactionsCache &transactionsCache);

        void serialize(std::ostream &stream, const std::string &password, bool saveDetailed, const std::string &cache);

        void deserialize(std::istream &stream, const std::string &password, std::string &cache);

    private:
        void saveKeys(CryptoNote::ISerializer &serializer);

        void loadKeys(CryptoNote::ISerializer &serializer);

        Crypto::chacha8_iv encrypt(const std::string &plain, const std::string &password, std::string &cipher);

        void decrypt(const std::string &cipher, std::string &plain, Crypto::chacha8_iv iv, const std::string &password);

        CryptoNote::AccountBase &account;
        WalletUserTransactionsCache &transactionsCache;
        const uint32_t walletSerializationVersion;
    };

} //namespace CryptoNote
