// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoTypes.h"

#include "json.hpp"

#include <NodeRpcProxy/NodeRpcProxy.h>

#include <string>

#include <tuple>

#include <vector>

#include <WalletBackend/WalletErrors.h>

using nlohmann::json;

/* Adds the given magic identifier to the string data */
void prependMagicIdentifier(std::string &data,
                            const uint8_t* magicIdentifier,
                            const size_t magicIdentifierSize);

class WalletBackend
{
    public:
        /* Imports a wallet from a mnemonic seed. Returns the wallet class,
           or an error. */
        static std::tuple<WalletError, WalletBackend> importWalletFromSeed(
            const std::string mnemonicSeed, const std::string filename,
            const std::string password, const uint64_t scanHeight,
            const std::string daemonHost, const uint16_t daemonPort
        );

        /* Imports a wallet from a private spend key and a view key. Returns
           the wallet class, or an error. */
        static std::tuple<WalletError, WalletBackend> importWalletFromKeys(
            const Crypto::SecretKey privateViewKey,
            const Crypto::SecretKey privateSpendKey, const std::string filename,
            const std::string password, const uint64_t scanHeight,
            const std::string daemonHost, const uint16_t daemonPort
        );

        /* Imports a view wallet from a private view key and an address.
           Returns the wallet class, or an error. */
        static std::tuple<WalletError, WalletBackend> importViewWallet(
            const Crypto::SecretKey privateViewKey, const std::string address,
            const std::string filename, const std::string password,
            const uint64_t scanHeight, const std::string daemonHost,
            const uint16_t daemonPort
        );

        /* Creates a new wallet with the given filename and password */
        static std::tuple<WalletError, WalletBackend> createWallet(
            const std::string filename, const std::string password,
            const std::string daemonHost, const uint16_t daemonPort
        );

        /* Opens a wallet already on disk with the given filename + password */
        static std::tuple<WalletError, WalletBackend> openWallet(
            const std::string filename, const std::string password,
            const std::string daemonHost, const uint16_t daemonPort
        );

        WalletError save() const;

    private:
        WalletBackend() {};

        json toJson() const;

        WalletError fromJson(std::string jsonStr);

        /* The filename the wallet is saved to */
        std::string m_filename;

        /* The password the wallet is encrypted with */
        std::string m_password;

        /* Each wallet has one shared private view key */
        Crypto::SecretKey m_privateViewKey;

        /* Each individual wallet has a separate private spend key */
        std::vector<Crypto::SecretKey> m_privateSpendKeys;

        /* Each wallet address is a combo of the shared private view key, and
           the individual private spend key */
        std::vector<std::string> m_addresses;

        /* A view wallet has a private view key, and a single address */
        bool m_isViewWallet;

        /* The daemon connection */
        std::shared_ptr<CryptoNote::NodeRpcProxy> m_daemon;

        /* We use this to check that the file is a wallet file, this bit does
           not get encrypted, and we can check if it exists before decrypting.
           If it isn't, it's not a wallet file. */
        static constexpr uint8_t isAWalletIdentifier[] =
        {
            0x49, 0x66, 0x20, 0x49, 0x20, 0x70, 0x75, 0x6c, 0x6c, 0x20, 0x74,
            0x68, 0x61, 0x74, 0x20, 0x6f, 0x66, 0x66, 0x2c, 0x20, 0x77, 0x69,
            0x6c, 0x6c, 0x20, 0x79, 0x6f, 0x75, 0x20, 0x64, 0x69, 0x65, 0x3f,
            0x0a, 0x49, 0x74, 0x20, 0x77, 0x6f, 0x75, 0x6c, 0x64, 0x20, 0x62,
            0x65, 0x20, 0x65, 0x78, 0x74, 0x72, 0x65, 0x6d, 0x65, 0x6c, 0x79,
            0x20, 0x70, 0x61, 0x69, 0x6e, 0x66, 0x75, 0x6c, 0x2e
        };

        /* We use this to check if the file has been correctly decoded, i.e.
           is the password correct. This gets encrypted into the file, and
           then when unencrypted the file should start with this - if it
           doesn't, the password is wrong */
        static constexpr uint8_t isCorrectPasswordIdentifier[] =
        {
            0x59, 0x6f, 0x75, 0x27, 0x72, 0x65, 0x20, 0x61, 0x20, 0x62, 0x69,
            0x67, 0x20, 0x67, 0x75, 0x79, 0x2e, 0x0a, 0x46, 0x6f, 0x72, 0x20,
            0x79, 0x6f, 0x75, 0x2e
        };

        /* The number of iterations of PBKDF2 to perform on the wallet
           password. */
        static constexpr uint64_t PBKDF2_ITERATIONS = 500000;

        /* What version of the file format are we on (to make it easier to
           upgrade the wallet format in the future) */
        static constexpr uint16_t WALLET_FILE_FORMAT_VERSION = 0;
};
