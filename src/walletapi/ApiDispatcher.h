// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "httplib.h"

#include <cryptopp/modes.h>
#include <walletbackend/WalletBackend.h>

enum WalletState
{
    WalletMustBeOpen,
    WalletMustBeClosed,
    DoesntMatter,
};

/* Functions the same as body.at(key).get<T>(), but gives better error messages */
template<typename T> T getJsonValue(const nlohmann::json &body, const std::string key)
{
    if (body.find(key) == body.end())
    {
        auto exception = json::type_error::create(304, "\nExpected json parameter '" + key + "' does not exist.");
        throw exception;
    }

    /* Could exist, but be wrong format */
    try
    {
        return body.at(key).get<T>();
    }
    catch (const json::exception &e)
    {
        std::string msg = "\nJson parameter '" + key
                          + "' exists, but we failed to parse it.\n"
                            "Possibly wrong type? ("
                          + e.what() + ")";

        auto exception = json::type_error::create(e.id, msg);

        throw exception;
    }
}

class ApiDispatcher
{
  public:
    //////////////////
    /* Constructors */
    //////////////////

    ApiDispatcher(
        const uint16_t bindPort,
        const std::string rpcBindIp,
        const std::string rpcPassword,
        std::string corsHeader,
        unsigned int walletSyncThreads = std::thread::hardware_concurrency());

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
        const WalletState walletState,
        const bool viewWalletsPermitted,
        std::function<std::tuple<Error, uint16_t>(
            const httplib::Request &req,
            httplib::Response &res,
            const nlohmann::json &body)> handler);

    /* Verifies that the request has the correct X-API-KEY, and sends a 401
       if it is not. */
    bool checkAuthenticated(const httplib::Request &req, httplib::Response &res) const;

    ///////////////////
    /* POST REQUESTS */
    ///////////////////

    /* Opens a wallet */
    std::tuple<Error, uint16_t>
        openWallet(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Imports a wallet using a private spend + private view key */
    std::tuple<Error, uint16_t>
        keyImportWallet(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Imports a wallet using a mnemonic seed */
    std::tuple<Error, uint16_t>
        seedImportWallet(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Imports a view only wallet using a private view key + address */
    std::tuple<Error, uint16_t>
        importViewWallet(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Creates a new wallet, which will be a deterministic wallet */
    std::tuple<Error, uint16_t>
        createWallet(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Create a new random address */
    std::tuple<Error, uint16_t>
        createAddress(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Imports an address with a private spend key */
    std::tuple<Error, uint16_t>
        importAddress(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Imports a deterministic address using a wallet index */
    std::tuple<Error, uint16_t>
        importDeterministicAddress(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Imports a view only address with a public spend key */
    std::tuple<Error, uint16_t>
        importViewAddress(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Validate an address or integrated address */
    std::tuple<Error, uint16_t>
        validateAddress(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Send a previously prepared transaction */
    std::tuple<Error, uint16_t>
        sendPreparedTransaction(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Prepare (don't send) a basic transaction */
    std::tuple<Error, uint16_t>
        prepareBasicTransaction(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Send a basic transaction */
    std::tuple<Error, uint16_t>
        sendBasicTransaction(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Make a basic transaction, optionally relaying to the network */
    std::tuple<Error, uint16_t>
        makeBasicTransaction(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body, const bool sendTransaction);

    /* Prepare (don't send) an advanced transaction */
    std::tuple<Error, uint16_t>
        prepareAdvancedTransaction(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Send an advanced transaction */
    std::tuple<Error, uint16_t>
        sendAdvancedTransaction(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Make an advanced transaction, optionally relaying to the network */
    std::tuple<Error, uint16_t>
        makeAdvancedTransaction(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body, const bool sendTransaction);

    /* Send a basic fusion transaction */
    std::tuple<Error, uint16_t>
        sendBasicFusionTransaction(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Send a more customizable fusion transaction */
    std::tuple<Error, uint16_t>
        sendAdvancedFusionTransaction(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Export wallet to file in JSON format */
    std::tuple<Error, uint16_t>
        exportToJSON(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /////////////////////
    /* DELETE REQUESTS */
    /////////////////////

    /* Close and save the wallet */
    std::tuple<Error, uint16_t>
        closeWallet(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        deleteAddress(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    std::tuple<Error, uint16_t>
        deletePreparedTransaction(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    //////////////////
    /* PUT REQUESTS */
    //////////////////

    /* Saves the wallet (Note - interrupts syncing for a short time) */
    std::tuple<Error, uint16_t>
        saveWallet(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body) const;

    /* Resets and saves the wallet */
    std::tuple<Error, uint16_t>
        resetWallet(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    /* Sets the daemon node and port */
    std::tuple<Error, uint16_t>
        setNodeInfo(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body);

    //////////////////
    /* GET REQUESTS */
    //////////////////

    /* Gets the node we are currently connected to, and its fee */
    std::tuple<Error, uint16_t>
        getNodeInfo(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body) const;

    /* Gets the shared private view key */
    std::tuple<Error, uint16_t>
        getPrivateViewKey(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body) const;

    /* Gets the spend keys for the given address */
    std::tuple<Error, uint16_t>
        getSpendKeys(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body) const;

    /* Gets the mnemonic seed for the given address (if possible) */
    std::tuple<Error, uint16_t>
        getMnemonicSeed(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body) const;

    /* Returns sync status, peer count, etc */
    std::tuple<Error, uint16_t>
        getStatus(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body) const;

    std::tuple<Error, uint16_t>
        getAddresses(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body) const;

    std::tuple<Error, uint16_t>
        getPrimaryAddress(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body) const;

    std::tuple<Error, uint16_t>
        createIntegratedAddress(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body) const;

    std::tuple<Error, uint16_t>
        getTransactions(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body) const;

    std::tuple<Error, uint16_t> getUnconfirmedTransactions(
        const httplib::Request &req,
        httplib::Response &res,
        const nlohmann::json &body) const;

    std::tuple<Error, uint16_t> getUnconfirmedTransactionsForAddress(
        const httplib::Request &req,
        httplib::Response &res,
        const nlohmann::json &body) const;

    std::tuple<Error, uint16_t> getTransactionsFromHeight(
        const httplib::Request &req,
        httplib::Response &res,
        const nlohmann::json &body) const;

    std::tuple<Error, uint16_t> getTransactionsFromHeightToHeight(
        const httplib::Request &req,
        httplib::Response &res,
        const nlohmann::json &body) const;

    std::tuple<Error, uint16_t> getTransactionsFromHeightWithAddress(
        const httplib::Request &req,
        httplib::Response &res,
        const nlohmann::json &body) const;

    std::tuple<Error, uint16_t> getTransactionsFromHeightToHeightWithAddress(
        const httplib::Request &req,
        httplib::Response &res,
        const nlohmann::json &body) const;

    std::tuple<Error, uint16_t>
        getTransactionDetails(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body) const;

    std::tuple<Error, uint16_t> getTransactionsByPaymentId(
        const httplib::Request &req,
        httplib::Response &res,
        const nlohmann::json &body) const;

    std::tuple<Error, uint16_t> getTransactionsWithPaymentId(
        const httplib::Request &req,
        httplib::Response &res,
        const nlohmann::json &body) const;

    std::tuple<Error, uint16_t>
        getBalance(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body) const;

    std::tuple<Error, uint16_t>
        getBalanceForAddress(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body) const;

    std::tuple<Error, uint16_t>
        getBalances(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body) const;

    std::tuple<Error, uint16_t>
        getTxPrivateKey(const httplib::Request &req, httplib::Response &res, const nlohmann::json &body) const;

    //////////////////////
    /* OPTIONS REQUESTS */
    //////////////////////

    /* Handles an OPTIONS request */
    void handleOptions(const httplib::Request &req, httplib::Response &res) const;

    //////////////////////////
    /* END OF API FUNCTIONS */
    //////////////////////////

    /* Extracts {host, port, ssl, filename, password}, from body */
    std::tuple<std::string, uint16_t, bool, std::string, std::string>
        getDefaultWalletParams(const nlohmann::json body) const;

    /* Assert the wallet is not a view only wallet */
    bool assertIsNotViewWallet() const;

    /* Assert the wallet is a view wallet */
    bool assertIsViewWallet() const;

    /* Assert the wallet is closed */
    bool assertWalletClosed() const;

    /* Assert the wallet is open */
    bool assertWalletOpen() const;

    /* Converts a public spend key to an address in a transactions json */
    void publicKeysToAddresses(nlohmann::json &j) const;

    std::string hashPassword(const std::string password) const;

    //////////////////////////////
    /* Private member variables */
    //////////////////////////////

    std::shared_ptr<WalletBackend> m_walletBackend = nullptr;

    /* Our server instance */
    httplib::Server m_server;

    /* The --rpc-password hashed with pbkdf2 */
    std::string m_hashedPassword;

    /* The rpc password - only stored to help indicate invalid passwords */
    std::string m_rpcPassword;

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

    /* Used along with our password with pbkdf2 */
    CryptoPP::byte m_salt[16];

    /* Amount of threads to use during wallet syncing */
    unsigned int m_walletSyncThreads;
};
