// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "LegacyKeysImporter.h"

#include <vector>
#include <system_error>

#include "Common/StringTools.h"

#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/Account.h"
#include "CryptoNoteCore/CryptoNoteTools.h"

#include "Serialization/SerializationTools.h"

#include "WalletLegacy/WalletLegacySerializer.h"
#include "WalletLegacy/WalletUserTransactionsCache.h"
#include "Wallet/WalletUtils.h"
#include "Wallet/WalletErrors.h"

using namespace Crypto;

namespace
{

    struct keys_file_data
    {
        chacha8_iv iv;
        std::string account_data;

        void serialize(CryptoNote::ISerializer &s)
        {
            s(iv, "iv");
            s(account_data, "account_data");
        }
    };

    void loadKeysFromFile(const std::string &filename, const std::string &password, CryptoNote::AccountBase &account)
    {
        keys_file_data keys_file_data;
        std::string buf;

        if (!Common::loadFileToString(filename, buf))
        {
            throw std::system_error(make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR),
                                    "failed to load \"" + filename + '\"');
        }

        if (!CryptoNote::fromBinaryArray(keys_file_data, Common::asBinaryArray(buf)))
        {
            throw std::system_error(make_error_code(CryptoNote::error::INTERNAL_WALLET_ERROR),
                                    "failed to deserialize \"" + filename + '\"');
        }

        chacha8_key key;
        cn_context cn_context;
        generate_chacha8_key(cn_context, password, key);
        std::string account_data;
        account_data.resize(keys_file_data.account_data.size());
        chacha8(keys_file_data.account_data.data(), keys_file_data.account_data.size(), key, keys_file_data.iv,
                &account_data[0]);

        if (!CryptoNote::loadFromBinaryKeyValue(account, account_data))
        {
            throw std::system_error(make_error_code(CryptoNote::error::WRONG_PASSWORD));
        }

        const CryptoNote::AccountKeys &keys = account.getAccountKeys();
        CryptoNote::throwIfKeysMismatch(keys.viewSecretKey, keys.address.viewPublicKey);
        CryptoNote::throwIfKeysMismatch(keys.spendSecretKey, keys.address.spendPublicKey);
    }

}

namespace CryptoNote
{

    void importLegacyKeys(const std::string &legacyKeysFilename, const std::string &password, std::ostream &destination)
    {
        CryptoNote::AccountBase account;

        loadKeysFromFile(legacyKeysFilename, password, account);

        CryptoNote::WalletUserTransactionsCache transactionsCache;
        std::string cache;
        CryptoNote::WalletLegacySerializer importer(account, transactionsCache);
        importer.serialize(destination, password, false, cache);
    }

} //namespace CryptoNote
