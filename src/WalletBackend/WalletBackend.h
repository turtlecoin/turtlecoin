// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoTypes.h"

#include "json.hpp"

#include <Logging/LoggerManager.h>

#include <NodeRpcProxy/NodeRpcProxy.h>

#include <string>

#include <tuple>

#include <vector>

#include <WalletBackend/SubWallets.h>
#include <WalletBackend/WalletErrors.h>
#include <WalletBackend/WalletSynchronizer.h>

using nlohmann::json;

class WalletBackend
{
    public:
        /* Very heavily suggested to not call this directly. Call one of the
           below functions to correctly initialize a wallet. This is left
           public so the json serialization works correctly. */
        WalletBackend();

        /* Deconstructor */
        ~WalletBackend();

        /* Delete the copy constructor */
        WalletBackend(const WalletBackend &) = delete;

        /* Delete the Assignment operator */
        WalletBackend & operator=(const WalletBackend &) = delete;

        /* Move constructor */
        WalletBackend(WalletBackend && old);

        /* Move Assignment Operator */
        WalletBackend & operator=(WalletBackend && old);

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
            const Crypto::SecretKey privateSpendKey,
            const Crypto::SecretKey privateViewKey, const std::string filename,
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

        /* Converts the class to a json object */
        json toJson() const;

        /* Initializes the class from a json string */
        void fromJson(const json &j);

    private:
        WalletBackend(std::string filename, std::string password,
                      Crypto::SecretKey privateSpendKey,
                      Crypto::SecretKey privateViewKey, bool isViewWallet,
                      uint64_t scanHeight, bool newWallet,
                      std::string daemonHost, uint16_t daemonPort);

        WalletError init();

        WalletError initializeAfterLoad(std::string filename,
                                        std::string password,
                                        std::string daemonHost,
                                        uint16_t daemonPort);

        void sync();

        /* The filename the wallet is saved to */
        std::string m_filename;

        /* The password the wallet is encrypted with */
        std::string m_password;

        /* Each subwallet shares one private view key */
        Crypto::SecretKey m_privateViewKey;

        /* A mapping of addresses to sub wallets (Using a shared_ptr here so
           the WalletSynchronizer has access to it) */
        std::shared_ptr<SubWallets> m_subWallets;

        /* A view wallet has a private view key, and a single address */
        bool m_isViewWallet;

        /* The daemon connection */
        std::shared_ptr<CryptoNote::NodeRpcProxy> m_daemon;

        /* The log manager */
        std::shared_ptr<Logging::LoggerManager> m_logManager;

        /* The logger instance (Need to keep around because the daemon
           constructor takes a reference to the variable, so if it goes out
           of scope we segfault... :facepalm: */
        std::shared_ptr<Logging::LoggerRef> m_logger;

        /* We use a shared pointer here, because we start the thread in the
           class, with the class as a context, hence, when we go to move the
           WalletSynchronizer class, the thread gets moved() across, but it
           is still pointing to a class which has been moved from, which
           is undefined behaviour. So, none of our changes to the
           WalletSynchronizer class reflect in the thread.

           The ideal way to fix this would probably to disable move semantics,
           and just assign once - however this is pretty tricky to do, as
           we want to use the factory pattern so we're not initializing
           with crappy data, and can return a meaningful error to the user
           rather than having to throw() or check isInitialized() everywhere.

           More info here: https://stackoverflow.com/q/43203869/8737306
           
           PS: I want to die */
        std::shared_ptr<WalletSynchronizer> m_walletSynchronizer;

        /* We use this to check that the file is a wallet file, this bit does
           not get encrypted, and we can check if it exists before decrypting.
           If it isn't, it's not a wallet file. */
        static constexpr std::array<uint8_t, 64> isAWalletIdentifier =
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
        static constexpr std::array<uint8_t, 26> isCorrectPasswordIdentifier =
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
