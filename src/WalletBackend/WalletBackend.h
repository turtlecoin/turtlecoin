// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoTypes.h"

#include <Errors/Errors.h>

#include "rapidjson/document.h"

#include <string>

#include <tuple>

#include <vector>

#include <Nigel/Nigel.h>

#include <SubWallets/SubWallets.h>

#include <WalletBackend/WalletSynchronizer.h>
#include <WalletBackend/WalletSynchronizerRAIIWrapper.h>

class WalletBackend
{
    public:

        /////////////////////////
        /* Public Constructors */
        /////////////////////////

        /* Very heavily suggested to not call this directly. Call one of the
           below functions to correctly initialize a wallet. This is left
           public so the json serialization works correctly. */
        WalletBackend();

        /* Deconstructor */
        ~WalletBackend();

        /* Delete the copy constructor */
        WalletBackend(const WalletBackend &) = delete;

        /* Delete the assignment operator */
        WalletBackend & operator=(const WalletBackend &) = delete;

        /* Delete the move constructor */
        WalletBackend(WalletBackend && old) = delete;

        /* Delete the move assignment operator */
        WalletBackend & operator=(WalletBackend && old) = delete;

        /////////////////////////////
        /* Public static functions */
        /////////////////////////////

        /* Imports a wallet from a mnemonic seed. Returns the wallet class,
           or an error. */
        static std::tuple<Error, std::shared_ptr<WalletBackend>> importWalletFromSeed(
            const std::string mnemonicSeed,
            const std::string filename,
            const std::string password,
            const uint64_t scanHeight,
            const std::string daemonHost,
            const uint16_t daemonPort,
            const bool daemonSSL);

        /* Imports a wallet from a private spend key and a view key. Returns
           the wallet class, or an error. */
        static std::tuple<Error, std::shared_ptr<WalletBackend>> importWalletFromKeys(
            const Crypto::SecretKey privateSpendKey,
            const Crypto::SecretKey privateViewKey,
            const std::string filename,
            const std::string password,
            const uint64_t scanHeight,
            const std::string daemonHost,
            const uint16_t daemonPort,
            const bool daemonSSL);

        /* Imports a view wallet from a private view key and an address.
           Returns the wallet class, or an error. */
        static std::tuple<Error, std::shared_ptr<WalletBackend>> importViewWallet(
            const Crypto::SecretKey privateViewKey,
            const std::string address,
            const std::string filename,
            const std::string password,
            const uint64_t scanHeight,
            const std::string daemonHost,
            const uint16_t daemonPort,
            const bool daemonSSL);

        /* Creates a new wallet with the given filename and password */
        static std::tuple<Error, std::shared_ptr<WalletBackend>> createWallet(
            const std::string filename,
            const std::string password,
            const std::string daemonHost,
            const uint16_t daemonPort,
            const bool daemonSSL);

        /* Opens a wallet already on disk with the given filename + password */
        static std::tuple<Error, std::shared_ptr<WalletBackend>> openWallet(
            const std::string filename,
            const std::string password,
            const std::string daemonHost,
            const uint16_t daemonPort,
            const bool daemonSSL);

        /////////////////////////////
        /* Public member functions */
        /////////////////////////////

        /* Save the wallet to disk */
        Error save() const;

        /* Converts the class to a json string */
        std::string toJSON() const;

        /* Initializes the class from a json string */
        Error fromJSON(const rapidjson::Document &j);

        /* Initializes the class from a json string, and inits the stuff we
           can't init from the json */
        Error fromJSON(
            const rapidjson::Document &j,
            const std::string filename,
            const std::string password,
            const std::string daemonHost,
            const uint16_t daemonPort,
            const bool daemonSSL);

        /* Send a transaction of amount to destination with paymentID */
        std::tuple<Error, Crypto::Hash> sendTransactionBasic(
            const std::string destination,
            const uint64_t amount,
            const std::string paymentID);

        /* Advanced send transaction, specify mixin, change address, etc */
        std::tuple<Error, Crypto::Hash> sendTransactionAdvanced(
            const std::vector<std::pair<std::string, uint64_t>> destinations,
            const uint64_t mixin,
            const uint64_t fee,
            const std::string paymentID,
            const std::vector<std::string> subWalletsToTakeFrom,
            const std::string changeAddress,
            const uint64_t unlockTime);

        /* Send a fusion using default mixin, default destination, and
           taking from all subwallets */
        std::tuple<Error, Crypto::Hash> sendFusionTransactionBasic();

        /* Send a fusion with advanced options */
        std::tuple<Error, Crypto::Hash> sendFusionTransactionAdvanced(
            const uint64_t mixin,
            const std::vector<std::string> subWalletsToTakeFrom,
            const std::string destinationAddress);

        /* Get the balance for one subwallet (error, unlocked, locked) */
        std::tuple<Error, uint64_t, uint64_t> getBalance(
            const std::string address) const;

        /* Get the balance for all subwallets */
        std::tuple<uint64_t, uint64_t> getTotalBalance() const;

        uint64_t getTotalUnlockedBalance() const;

        /* Make a new sub wallet (gens a privateSpendKey) */
        std::tuple<Error, std::string, Crypto::SecretKey> addSubWallet();

        /* Import a sub wallet with the given privateSpendKey */
        std::tuple<Error, std::string> importSubWallet(
            const Crypto::SecretKey privateSpendKey,
            const uint64_t scanHeight);

        /* Import a view only sub wallet with the given publicSpendKey */
        std::tuple<Error, std::string> importViewSubWallet(
            const Crypto::PublicKey publicSpendKey,
            const uint64_t scanHeight);

        Error deleteSubWallet(const std::string address);

        /* Scan the blockchain, starting from scanHeight / timestamp */
        void reset(uint64_t scanHeight, uint64_t timestamp);

        /* Is the wallet a view only wallet */
        bool isViewWallet() const;

        /* Get the filename of the wallet on disk */
        std::string getWalletLocation() const;

        /* Get the primary address */
        std::string getPrimaryAddress() const;

        /* Get a list of all addresses in the wallet */
        std::vector<std::string> getAddresses() const;

        uint64_t getWalletCount() const;

        /* wallet sync height, local blockchain sync height,
           remote blockchain sync height */
        std::tuple<uint64_t, uint64_t, uint64_t> getSyncStatus() const;

        /* Get the wallet password */
        std::string getWalletPassword() const;

        /* Change the wallet password and save the wallet with the new password */
        Error changePassword(const std::string newPassword);

        /* Gets the shared private view key */
        Crypto::SecretKey getPrivateViewKey() const;

        /* Gets the public and private spend key for the given address */
        std::tuple<Error, Crypto::PublicKey, Crypto::SecretKey>
            getSpendKeys(const std::string &address) const;

        /* Get the private spend and private view for the primary address */
        std::tuple<Crypto::SecretKey, Crypto::SecretKey> getPrimaryAddressPrivateKeys() const;

        /* Get the primary address mnemonic seed, if possible */
        std::tuple<Error, std::string> getMnemonicSeed() const;

        /* Gets the mnemonic seed for the given address, if possible */
        std::tuple<Error, std::string> getMnemonicSeedForAddress(
            const std::string &address) const;

        /* Get all transactions */
        std::vector<WalletTypes::Transaction> getTransactions() const;

        /* Get all unconfirmed (outgoing, sent) transactions */
        std::vector<WalletTypes::Transaction> getUnconfirmedTransactions() const;

        /* Get sync heights, hashrate, peer count */
        WalletTypes::WalletStatus getStatus() const;

        /* Returns transactions in the range [startHeight, endHeight - 1] - so if
           we give 1, 100, it will return transactions from block 1 to block 99 */
        std::vector<WalletTypes::Transaction> getTransactionsRange(
            const uint64_t startHeight, const uint64_t endHeight) const;

        /* Get the node fee and address ({0, ""} if empty) */
        std::tuple<uint64_t, std::string> getNodeFee() const;

        /* Returns the node host and port */
        std::tuple<std::string, uint16_t, bool> getNodeAddress() const;

        /* Swap to a different daemon node */
        void swapNode(std::string daemonHost, uint16_t daemonPort, bool daemonSSL);

        /* Whether we have recieved info from the daemon at some point */
        bool daemonOnline() const;

        std::tuple<Error, std::string> getAddress(
            const Crypto::PublicKey spendKey) const;

        std::tuple<Error, Crypto::SecretKey> getTxPrivateKey(
            const Crypto::Hash txHash) const;

        std::vector<std::tuple<std::string, uint64_t, uint64_t>> getBalances() const;

        /////////////////////////////
        /* Public member variables */
        /////////////////////////////

        std::shared_ptr<EventHandler> m_eventHandler;

    private:

        //////////////////////////
        /* Private constructors */
        //////////////////////////

        /* Standard Constructor */
        WalletBackend(
            const std::string filename,
            const std::string password,
            const Crypto::SecretKey privateSpendKey,
            const Crypto::SecretKey privateViewKey,
            const uint64_t scanHeight,
            const bool newWallet,
            const std::string daemonHost,
            const uint16_t daemonPort,
            const bool daemonSSL);

        /* View wallet constructor */
        WalletBackend(
            const std::string filename,
            const std::string password,
            const Crypto::SecretKey privateViewKey,
            const std::string address,
            const uint64_t scanHeight,
            const std::string daemonHost,
            const uint16_t daemonPort,
            const bool daemonSSL);

        //////////////////////////////
        /* Private member functions */
        //////////////////////////////

        Error unsafeSave() const;

        void init();

        //////////////////////////////
        /* Private member variables */
        //////////////////////////////

        /* The filename the wallet is saved to */
        std::string m_filename;

        /* The password the wallet is encrypted with */
        std::string m_password;

        /* The sub wallets container (Using a shared_ptr here so
           the WalletSynchronizer has access to it) */
        std::shared_ptr<SubWallets> m_subWallets;

        /* The daemon connection */
        std::shared_ptr<Nigel> m_daemon = nullptr;

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

        std::shared_ptr<WalletSynchronizerRAIIWrapper> m_syncRAIIWrapper;
};
