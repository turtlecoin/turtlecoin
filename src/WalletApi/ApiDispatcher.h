// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <WalletBackend/WalletBackend.h>

#include "httplib.h"

class ApiDispatcher
{
    public:
        //////////////////
        /* Constructors */
        //////////////////

        ApiDispatcher(
            const uint16_t bindPort,
            const bool acceptExternalRequests,
            const std::string rpcPassword,
            std::string corsHeader);

        /////////////////////////////
        /* Public member functions */
        /////////////////////////////

        /* Starts the server */
        void start();

        /* Stops the server */
        void stop();
        
    private:

        //////////////////////////////
        /* Private member functions */
        //////////////////////////////

        /* Check authentication and log, then forward on to the handler if
           applicable */
        void middleware(
            const httplib::Request &req,
            httplib::Response &res,
            const bool walletMustBeOpen,
            const bool viewWalletsPermitted,
            std::function<std::tuple<WalletError, uint16_t>
                (const httplib::Request &req,
                 httplib::Response &res,
                 const nlohmann::json &body)> handler);

        /* Verifies that the request has the correct X-API-KEY, and sends a 401
           if it is not. */
        bool checkAuthenticated(
            const httplib::Request &req,
            httplib::Response &res) const;

        ///////////////////
        /* POST REQUESTS */
        ///////////////////
        
        /* Opens a wallet */
        std::tuple<WalletError, uint16_t> openWallet(
            const httplib::Request &req,
            httplib::Response &res,
            const nlohmann::json &body);

        /* Imports a wallet using a private spend + private view key */
        std::tuple<WalletError, uint16_t> keyImportWallet(
            const httplib::Request &req,
            httplib::Response &res,
            const nlohmann::json &body);

        /* Imports a wallet using a mnemonic seed */
        std::tuple<WalletError, uint16_t> seedImportWallet(
            const httplib::Request &req,
            httplib::Response &res,
            const nlohmann::json &body);

        /* Imports a view only wallet using a private view key + address */
        std::tuple<WalletError, uint16_t> importViewWallet(
            const httplib::Request &req,
            httplib::Response &res,
            const nlohmann::json &body);

        /* Creates a new wallet, which will be a deterministic wallet */
        std::tuple<WalletError, uint16_t> createWallet(
            const httplib::Request &req,
            httplib::Response &res,
            const nlohmann::json &body);

        /////////////////////
        /* DELETE REQUESTS */
        /////////////////////

        /* Close and save the wallet */
        std::tuple<WalletError, uint16_t> closeWallet(
            const httplib::Request &req,
            httplib::Response &res,
            const nlohmann::json &body);

        //////////////////
        /* PUT REQUESTS */
        //////////////////

        /* Saves the wallet (Note - interrupts syncing for a short time) */
        std::tuple<WalletError, uint16_t> saveWallet(
            const httplib::Request &req,
            httplib::Response &res,
            const nlohmann::json &body) const;

        /* Resets and saves the wallet */
        std::tuple<WalletError, uint16_t> resetWallet(
            const httplib::Request &req,
            httplib::Response &res,
            const nlohmann::json &body);

        /* Sets the daemon node and port */
        std::tuple<WalletError, uint16_t> setNodeInfo(
            const httplib::Request &req,
            httplib::Response &res,
            const nlohmann::json &body);

        //////////////////
        /* GET REQUESTS */
        //////////////////

        /* Gets the node we are currently connected to, and its fee */
        std::tuple<WalletError, uint16_t> getNodeInfo(
            const httplib::Request &req,
            httplib::Response &res,
            const nlohmann::json &body) const;

        /* Gets the shared private view key */
        std::tuple<WalletError, uint16_t> getPrivateViewKey(
            const httplib::Request &req,
            httplib::Response &res,
            const nlohmann::json &body) const;

        /* Gets the spend keys for the given address */
        std::tuple<WalletError, uint16_t> getSpendKeys(
            const httplib::Request &req,
            httplib::Response &res,
            const nlohmann::json &body) const;

        /* Gets the mnemonic seed for the given address (if possible) */
        std::tuple<WalletError, uint16_t> getMnemonicSeed(
            const httplib::Request &req,
            httplib::Response &res,
            const nlohmann::json &body) const;

        /* Returns sync status, peer count, etc */
        std::tuple<WalletError, uint16_t> getStatus(
            const httplib::Request &req,
            httplib::Response &res,
            const nlohmann::json &body) const;

        std::tuple<WalletError, uint16_t> getAddresses(
            const httplib::Request &req,
            httplib::Response &res,
            const nlohmann::json &body) const;

        //////////////////////
        /* OPTIONS REQUESTS */
        //////////////////////

        /* Handles an OPTIONS request */
        void handleOptions(
            const httplib::Request &req,
            httplib::Response &res) const;

        //////////////////////////
        /* END OF API FUNCTIONS */
        //////////////////////////

        /* Extracts {host, port, filename, password}, from body */
        std::tuple<std::string, uint16_t, std::string, std::string>
            getDefaultWalletParams(const nlohmann::json body) const;

        /* Assert the wallet is not a view only wallet */
        bool assertIsNotViewWallet() const;

        /* Assert the wallet is closed */
        bool assertWalletClosed() const;

        /* Assert the wallet is open */
        bool assertWalletOpen() const;
        
        //////////////////////////////
        /* Private member variables */
        //////////////////////////////

        std::shared_ptr<WalletBackend> m_walletBackend = nullptr;

        /* Our server instance */
        httplib::Server m_server;

        /* The --rpc-password hashed with pbkdf2 */
        std::string m_hashedPassword;

        /* Need a mutex for some actions, mainly mutating actions, like opening
           wallets, sending transfers, etc */
        mutable std::mutex m_mutex;

        /* The server host */
        std::string m_host;

        /* The server port */
        uint16_t m_port;

        /* The header to use with 'Access-Control-Allow-Origin'. If empty string,
           header is not added. */
        std::string m_corsHeader;
};