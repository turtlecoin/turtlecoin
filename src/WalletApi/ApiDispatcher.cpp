// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

////////////////////////////////////
#include <WalletApi/ApiDispatcher.h>
////////////////////////////////////

#include <config/CryptoNoteConfig.h>

#include <CryptoNoteCore/Mixins.h>

#include <crypto/random.h>

#include <cryptopp/modes.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/sha.h>

#include <Errors/ValidateParameters.h>

#include <iomanip>

#include <iostream>

#include "json.hpp"

#include <WalletApi/Constants.h>

#include <WalletBackend/JsonSerialization.h>

using namespace httplib;

ApiDispatcher::ApiDispatcher(const uint16_t bindPort, const std::string rpcBindIp, const std::string rpcPassword,
                             const std::string corsHeader)
    : m_port(bindPort), m_host(rpcBindIp), m_corsHeader(corsHeader), m_rpcPassword(rpcPassword)
{
    /* Generate the salt used for pbkdf2 api authentication */
    Random::randomBytes(16, m_salt);

    /* Make sure to do this after initializing the salt above! */
    m_hashedPassword = hashPassword(rpcPassword);

    using namespace std::placeholders;

    /* Route the request through our middleware function, before forwarding
       to the specified function */
    const auto router = [this](const auto function, const bool walletMustBeOpen, const bool viewWalletPermitted) {
        return [=](const Request &req, Response &res) {
            /* Pass the inputted function with the arguments passed through
               to middleware */
            middleware(req, res, walletMustBeOpen, viewWalletPermitted, std::bind(function, this, _1, _2, _3));
        };
    };

    /* Makes the below router function easier to parse */
    const bool walletMustBeOpen = true;

    const bool walletMustBeClosed = false;

    const bool viewWalletsAllowed = true;

    const bool viewWalletsBanned = false;

    /* POST */
    m_server
        .Post("/wallet/open", router(&ApiDispatcher::openWallet, walletMustBeClosed, viewWalletsAllowed))

        /* Import wallet with keys */
        .Post("/wallet/import/key", router(&ApiDispatcher::keyImportWallet, walletMustBeClosed, viewWalletsAllowed))

        /* Import wallet with seed */
        .Post("/wallet/import/seed", router(&ApiDispatcher::seedImportWallet, walletMustBeClosed, viewWalletsAllowed))

        /* Import view wallet */
        .Post("/wallet/import/view", router(&ApiDispatcher::importViewWallet, walletMustBeClosed, viewWalletsAllowed))

        /* Create wallet */
        .Post("/wallet/create", router(&ApiDispatcher::createWallet, walletMustBeClosed, viewWalletsAllowed))

        /* Create a random address */
        .Post("/addresses/create", router(&ApiDispatcher::createAddress, walletMustBeOpen, viewWalletsBanned))

        /* Import an address with a spend secret key */
        .Post("/addresses/import", router(&ApiDispatcher::importAddress, walletMustBeOpen, viewWalletsBanned))

        /* Import a view only address with a public spend key */
        .Post("/addresses/import/view", router(&ApiDispatcher::importViewAddress, walletMustBeOpen, viewWalletsAllowed))

        /* Send a transaction */
        .Post("/transactions/send/basic",
              router(&ApiDispatcher::sendBasicTransaction, walletMustBeOpen, viewWalletsBanned))

        /* Send a transaction, more parameters specified */
        .Post("/transactions/send/advanced",
              router(&ApiDispatcher::sendAdvancedTransaction, walletMustBeOpen, viewWalletsBanned))

        /* Send a fusion transaction */
        .Post("/transactions/send/fusion/basic",
              router(&ApiDispatcher::sendBasicFusionTransaction, walletMustBeOpen, viewWalletsBanned))

        /* Send a fusion transaction, more parameters specified */
        .Post("/transactions/send/fusion/advanced",
              router(&ApiDispatcher::sendAdvancedFusionTransaction, walletMustBeOpen, viewWalletsBanned))

        /* DELETE */

        /* Close the current wallet */
        .Delete("/wallet", router(&ApiDispatcher::closeWallet, walletMustBeOpen, viewWalletsAllowed))

        /* Delete the given address */
        .Delete("/addresses/" + ApiConstants::addressRegex,
                router(&ApiDispatcher::deleteAddress, walletMustBeOpen, viewWalletsAllowed))

        /* PUT */

        /* Save the wallet */
        .Put("/save", router(&ApiDispatcher::saveWallet, walletMustBeOpen, viewWalletsAllowed))

        /* Reset the wallet from zero, or given scan height */
        .Put("/reset", router(&ApiDispatcher::resetWallet, walletMustBeOpen, viewWalletsAllowed))

        /* Swap node details */
        .Put("/node", router(&ApiDispatcher::setNodeInfo, walletMustBeOpen, viewWalletsAllowed))

        /* GET */

        /* Get node details */
        .Get("/node", router(&ApiDispatcher::getNodeInfo, walletMustBeOpen, viewWalletsAllowed))

        /* Get the shared private view key */
        .Get("/keys", router(&ApiDispatcher::getPrivateViewKey, walletMustBeOpen, viewWalletsAllowed))

        /* Get the spend keys for the given address */
        .Get("/keys/" + ApiConstants::addressRegex,
             router(&ApiDispatcher::getSpendKeys, walletMustBeOpen, viewWalletsBanned))

        /* Get the mnemonic seed for the given address */
        .Get("/keys/mnemonic/" + ApiConstants::addressRegex,
             router(&ApiDispatcher::getMnemonicSeed, walletMustBeOpen, viewWalletsBanned))

        /* Get the wallet status */
        .Get("/status", router(&ApiDispatcher::getStatus, walletMustBeOpen, viewWalletsAllowed))

        /* Get a list of all addresses */
        .Get("/addresses", router(&ApiDispatcher::getAddresses, walletMustBeOpen, viewWalletsAllowed))

        /* Get the primary address */
        .Get("/addresses/primary", router(&ApiDispatcher::getPrimaryAddress, walletMustBeOpen, viewWalletsAllowed))

        /* Creates an integrated address from the given address and payment ID */
        .Get("/addresses/" + ApiConstants::addressRegex + "/" + ApiConstants::hashRegex,
             router(&ApiDispatcher::createIntegratedAddress, walletMustBeOpen, viewWalletsAllowed))

        /* Get all transactions */
        .Get("/transactions", router(&ApiDispatcher::getTransactions, walletMustBeOpen, viewWalletsAllowed))

        /* Get all (outgoing) unconfirmed transactions */
        .Get("/transactions/unconfirmed",
             router(&ApiDispatcher::getUnconfirmedTransactions, walletMustBeOpen, viewWalletsAllowed))

        /* Get all (outgoing) unconfirmed transactions, belonging to the given address */
        .Get("/transactions/unconfirmed/" + ApiConstants::addressRegex,
             router(&ApiDispatcher::getUnconfirmedTransactionsForAddress, walletMustBeOpen, viewWalletsAllowed))

        /* Get the transactions starting at the given block, for 1000 blocks */
        .Get("/transactions/\\d+",
             router(&ApiDispatcher::getTransactionsFromHeight, walletMustBeOpen, viewWalletsAllowed))

        /* Get the transactions starting at the given block, and ending at the given block */
        .Get("/transactions/\\d+/\\d+",
             router(&ApiDispatcher::getTransactionsFromHeightToHeight, walletMustBeOpen, viewWalletsAllowed))

        /* Get the transactions starting at the given block, for 1000 blocks, belonging to the given address */
        .Get("/transactions/address/" + ApiConstants::addressRegex + "/\\d+",
             router(&ApiDispatcher::getTransactionsFromHeightWithAddress, walletMustBeOpen, viewWalletsAllowed))

        /* Get the transactions starting at the given block, and ending at the given block, belonging to the given
           address */
        .Get("/transactions/address/" + ApiConstants::addressRegex + "/\\d+/\\d+",
             router(&ApiDispatcher::getTransactionsFromHeightToHeightWithAddress, walletMustBeOpen, viewWalletsAllowed))

        /* Get the transaction private key for the given hash */
        .Get("/transactions/privatekey/" + ApiConstants::hashRegex,
             router(&ApiDispatcher::getTxPrivateKey, walletMustBeOpen, viewWalletsBanned))

        /* Get details for the given transaction hash, if known */
        .Get("/transactions/hash/" + ApiConstants::hashRegex,
             router(&ApiDispatcher::getTransactionDetails, walletMustBeOpen, viewWalletsAllowed))

        /* Get balance for the wallet */
        .Get("/balance", router(&ApiDispatcher::getBalance, walletMustBeOpen, viewWalletsAllowed))

        /* Get balance for a specific address */
        .Get("/balance/" + ApiConstants::addressRegex,
             router(&ApiDispatcher::getBalanceForAddress, walletMustBeOpen, viewWalletsAllowed))

        /* Get balances for each address */
        .Get("/balances", router(&ApiDispatcher::getBalances, walletMustBeOpen, viewWalletsAllowed))

        /* OPTIONS */

        /* Matches everything */
        /* NOTE: Not passing through middleware */
        .Options(".*", [this](auto &req, auto &res) { handleOptions(req, res); });
}

void ApiDispatcher::start()
{
    if (!m_server.listen(m_host, m_port))
    {
        std::cout << "Could not bind service to " << m_host << ":" << m_port
                  << "\nIs another service using this address and port?\n";
        exit(1);
    }
}

void ApiDispatcher::stop() { m_server.stop(); }

void ApiDispatcher::middleware(
    const Request &req, Response &res, const bool walletMustBeOpen, const bool viewWalletPermitted,
    std::function<std::tuple<Error, uint16_t>(const Request &req, Response &res, const nlohmann::json &body)> handler)
{
    std::cout << "Incoming " << req.method << " request: " << req.path << std::endl;

    nlohmann::json body;

    try
    {
        body = json::parse(req.body);
        std::cout << "Body:\n" << std::setw(4) << body << std::endl;
    }
    /* Not neccessarily an error if body isn't needed */
    catch (const json::exception &)
    {
        /* Body given, but failed to parse as JSON. Probably a mistake on
           the clients side, but lets report it to help them out. */
        if (!req.body.empty())
        {
            std::cout << "Warning: received body is not JSON encoded!\n"
                      << "Key/value parameters are NOT supported.\n"
                      << "Body:\n"
                      << req.body << std::endl;
        }
    }

    /* Add the cors header if not empty string */
    if (m_corsHeader != "")
    {
        res.set_header("Access-Control-Allow-Origin", m_corsHeader);
    }

    if (!checkAuthenticated(req, res))
    {
        return;
    }

    /* Wallet must be open for this operation, and it is not */
    if (walletMustBeOpen && !assertWalletOpen())
    {
        res.status = 403;
        return;
    }
    /* Wallet must not be open for this operation, and it is */
    else if (!walletMustBeOpen && !assertWalletClosed())
    {
        res.status = 403;
        return;
    }

    /* We have a wallet open, view wallets are not permitted, and the wallet is
       a view wallet (wew!) */
    if (m_walletBackend != nullptr && !viewWalletPermitted && !assertIsNotViewWallet())
    {
        /* Bad request */
        res.status = 400;

        Error error = ILLEGAL_VIEW_WALLET_OPERATION;

        nlohmann::json j{{"errorCode", error.getErrorCode()}, {"errorMessage", error.getErrorMessage()}};

        /* Pretty print ;o */
        res.set_content(j.dump(4) + "\n", "application/json");

        return;
    }

    try
    {
        const auto [error, statusCode] = handler(req, res, body);

        if (error)
        {
            /* Bad request */
            res.status = 400;

            nlohmann::json j{{"errorCode", error.getErrorCode()}, {"errorMessage", error.getErrorMessage()}};

            /* Pretty print ;o */
            res.set_content(j.dump(4) + "\n", "application/json");
        }
        else
        {
            res.status = statusCode;
        }
    }
    /* Most likely a key was missing. Do the error handling here to make the
       rest of the code simpler */
    catch (const json::exception &e)
    {
        std::cout << "Caught JSON exception, likely missing required "
                     "json parameter: "
                  << e.what() << std::endl;
        res.status = 400;
    }
    catch (const std::exception &e)
    {
        std::cout << "Caught unexpected exception: " << e.what() << std::endl;
        res.status = 500;
    }
}

bool ApiDispatcher::checkAuthenticated(const Request &req, Response &res) const
{
    if (!req.has_header("X-API-KEY"))
    {
        std::cout << "Rejecting unauthorized request: X-API-KEY header is missing.\n";

        /* Unauthorized */
        res.status = 401;
        return false;
    }

    std::string apiKey = req.get_header_value("X-API-KEY");

    if (hashPassword(apiKey) == m_hashedPassword)
    {
        return true;
    }

    std::cout << "Rejecting unauthorized request: X-API-KEY is incorrect.\n"
                 "Expected: "
              << m_rpcPassword << "\nActual: " << apiKey << std::endl;

    res.status = 401;

    return false;
}

///////////////////
/* POST REQUESTS */
///////////////////

std::tuple<Error, uint16_t> ApiDispatcher::openWallet(const Request &req, Response &res, const nlohmann::json &body)
{
    std::scoped_lock lock(m_mutex);

    const auto [daemonHost, daemonPort, filename, password] = getDefaultWalletParams(body);

    Error error;

    std::tie(error, m_walletBackend) = WalletBackend::openWallet(filename, password, daemonHost, daemonPort);

    return {error, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::keyImportWallet(const Request &req, Response &res,
                                                           const nlohmann::json &body)
{
    std::scoped_lock lock(m_mutex);

    const auto [daemonHost, daemonPort, filename, password] = getDefaultWalletParams(body);

    const auto privateViewKey = tryGetJsonValue<Crypto::SecretKey>(body, "privateViewKey");
    const auto privateSpendKey = tryGetJsonValue<Crypto::SecretKey>(body, "privateSpendKey");

    uint64_t scanHeight = 0;

    if (body.find("scanHeight") != body.end())
    {
        scanHeight = tryGetJsonValue<uint64_t>(body, "scanHeight");
    }

    Error error;

    std::tie(error, m_walletBackend) = WalletBackend::importWalletFromKeys(
        privateSpendKey, privateViewKey, filename, password, scanHeight, daemonHost, daemonPort);

    return {error, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::seedImportWallet(const Request &req, Response &res,
                                                            const nlohmann::json &body)
{
    std::scoped_lock lock(m_mutex);

    const auto [daemonHost, daemonPort, filename, password] = getDefaultWalletParams(body);

    const std::string mnemonicSeed = tryGetJsonValue<std::string>(body, "mnemonicSeed");

    uint64_t scanHeight = 0;

    if (body.find("scanHeight") != body.end())
    {
        scanHeight = tryGetJsonValue<uint64_t>(body, "scanHeight");
    }

    Error error;

    std::tie(error, m_walletBackend) =
        WalletBackend::importWalletFromSeed(mnemonicSeed, filename, password, scanHeight, daemonHost, daemonPort);

    return {error, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::importViewWallet(const Request &req, Response &res,
                                                            const nlohmann::json &body)
{
    std::scoped_lock lock(m_mutex);

    const auto [daemonHost, daemonPort, filename, password] = getDefaultWalletParams(body);

    const std::string address = tryGetJsonValue<std::string>(body, "address");
    const auto privateViewKey = tryGetJsonValue<Crypto::SecretKey>(body, "privateViewKey");

    uint64_t scanHeight = 0;

    if (body.find("scanHeight") != body.end())
    {
        scanHeight = tryGetJsonValue<uint64_t>(body, "scanHeight");
    }

    Error error;

    std::tie(error, m_walletBackend) = WalletBackend::importViewWallet(privateViewKey, address, filename, password,
                                                                       scanHeight, daemonHost, daemonPort);

    return {error, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::createWallet(const Request &req, Response &res, const nlohmann::json &body)
{
    std::scoped_lock lock(m_mutex);

    const auto [daemonHost, daemonPort, filename, password] = getDefaultWalletParams(body);

    Error error;

    std::tie(error, m_walletBackend) = WalletBackend::createWallet(filename, password, daemonHost, daemonPort);

    return {error, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::createAddress(const Request &req, Response &res, const nlohmann::json &body)
{
    const auto [error, address, privateSpendKey] = m_walletBackend->addSubWallet();

    nlohmann::json j{{"address", address}, {"privateSpendKey", privateSpendKey}};

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 201};
}

std::tuple<Error, uint16_t> ApiDispatcher::importAddress(const Request &req, Response &res, const nlohmann::json &body)
{
    uint64_t scanHeight = 0;

    /* Strongly suggested to supply a scan height. Wallet syncing will have to
       begin again from zero if none is given */
    if (body.find("scanHeight") != body.end())
    {
        scanHeight = tryGetJsonValue<uint64_t>(body, "scanHeight");
    }

    const auto privateSpendKey = tryGetJsonValue<Crypto::SecretKey>(body, "privateSpendKey");

    const auto [error, address] = m_walletBackend->importSubWallet(privateSpendKey, scanHeight);

    if (error)
    {
        return {error, 400};
    }

    nlohmann::json j{{"address", address}};

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 201};
}

std::tuple<Error, uint16_t> ApiDispatcher::importViewAddress(const Request &req, Response &res,
                                                             const nlohmann::json &body)
{
    uint64_t scanHeight = 0;

    /* Strongly suggested to supply a scan height. Wallet syncing will have to
       begin again from zero if none is given */
    if (body.find("scanHeight") != body.end())
    {
        scanHeight = tryGetJsonValue<uint64_t>(body, "scanHeight");
    }

    const auto publicSpendKey = tryGetJsonValue<Crypto::PublicKey>(body, "publicSpendKey");

    const auto [error, address] = m_walletBackend->importViewSubWallet(publicSpendKey, scanHeight);

    if (error)
    {
        return {error, 400};
    }

    nlohmann::json j{{"address", address}};

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 201};
}

std::tuple<Error, uint16_t> ApiDispatcher::sendBasicTransaction(const Request &req, Response &res,
                                                                const nlohmann::json &body)
{
    const std::string address = tryGetJsonValue<std::string>(body, "destination");

    const uint64_t amount = tryGetJsonValue<uint64_t>(body, "amount");

    std::string paymentID;

    if (body.find("paymentID") != body.end())
    {
        paymentID = tryGetJsonValue<std::string>(body, "paymentID");
    }

    auto [error, hash] = m_walletBackend->sendTransactionBasic(address, amount, paymentID);

    if (error)
    {
        return {error, 400};
    }

    nlohmann::json j{{"transactionHash", hash}};

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 201};
}

std::tuple<Error, uint16_t> ApiDispatcher::sendAdvancedTransaction(const Request &req, Response &res,
                                                                   const nlohmann::json &body)
{
    const json destinationsJSON = tryGetJsonValue<json>(body, "destinations");

    std::vector<std::pair<std::string, uint64_t>> destinations;

    for (const auto destination : destinationsJSON)
    {
        const std::string address = tryGetJsonValue<std::string>(destination, "address");
        const uint64_t amount = tryGetJsonValue<uint64_t>(destination, "amount");
        destinations.emplace_back(address, amount);
    }

    uint64_t mixin;

    if (body.find("mixin") != body.end())
    {
        mixin = tryGetJsonValue<uint64_t>(body, "mixin");
    }
    else
    {
        /* Get the default mixin */
        std::tie(std::ignore, std::ignore, mixin) =
            CryptoNote::Mixins::getMixinAllowableRange(m_walletBackend->getStatus().networkBlockCount);
    }

    uint64_t fee = WalletConfig::defaultFee;

    if (body.find("fee") != body.end())
    {
        fee = tryGetJsonValue<uint64_t>(body, "fee");
    }

    std::vector<std::string> subWalletsToTakeFrom = {};

    if (body.find("sourceAddresses") != body.end())
    {
        subWalletsToTakeFrom = tryGetJsonValue<std::vector<std::string>>(body, "sourceAddresses");
    }

    std::string paymentID;

    if (body.find("paymentID") != body.end())
    {
        paymentID = tryGetJsonValue<std::string>(body, "paymentID");
    }

    std::string changeAddress;

    if (body.find("changeAddress") != body.end())
    {
        changeAddress = tryGetJsonValue<std::string>(body, "changeAddress");
    }

    uint64_t unlockTime = 0;

    if (body.find("unlockTime") != body.end())
    {
        unlockTime = tryGetJsonValue<uint64_t>(body, "unlockTime");
    }

    auto [error, hash] = m_walletBackend->sendTransactionAdvanced(destinations, mixin, fee, paymentID,
                                                                  subWalletsToTakeFrom, changeAddress, unlockTime);

    if (error)
    {
        return {error, 400};
    }

    nlohmann::json j{{"transactionHash", hash}};

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::sendBasicFusionTransaction(const Request &req, Response &res,
                                                                      const nlohmann::json &body)
{
    auto [error, hash] = m_walletBackend->sendFusionTransactionBasic();

    if (error)
    {
        return {error, 400};
    }

    nlohmann::json j{{"transactionHash", hash}};

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 201};
}

std::tuple<Error, uint16_t> ApiDispatcher::sendAdvancedFusionTransaction(const Request &req, Response &res,
                                                                         const nlohmann::json &body)
{
    const std::string destination = tryGetJsonValue<std::string>(body, "destination");

    uint64_t mixin;

    if (body.find("mixin") != body.end())
    {
        mixin = tryGetJsonValue<uint64_t>(body, "mixin");
    }
    else
    {
        /* Get the default mixin */
        std::tie(std::ignore, std::ignore, mixin) =
            CryptoNote::Mixins::getMixinAllowableRange(m_walletBackend->getStatus().networkBlockCount);
    }

    std::vector<std::string> subWalletsToTakeFrom;

    if (body.find("sourceAddresses") != body.end())
    {
        subWalletsToTakeFrom = tryGetJsonValue<std::vector<std::string>>(body, "sourceAddresses");
    }

    auto [error, hash] = m_walletBackend->sendFusionTransactionAdvanced(mixin, subWalletsToTakeFrom, destination);

    if (error)
    {
        return {error, 400};
    }

    nlohmann::json j{{"transactionHash", hash}};

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 201};
}

/////////////////////
/* DELETE REQUESTS */
/////////////////////

std::tuple<Error, uint16_t> ApiDispatcher::closeWallet(const Request &req, Response &res, const nlohmann::json &body)
{
    std::scoped_lock lock(m_mutex);

    m_walletBackend = nullptr;

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::deleteAddress(const Request &req, Response &res, const nlohmann::json &body)
{
    /* Remove the addresses prefix to get the address */
    std::string address = req.path.substr(std::string("/addresses/").size());

    if (Error error = validateAddresses({address}, false); error != SUCCESS)
    {
        return {error, 400};
    }

    Error error = m_walletBackend->deleteSubWallet(address);

    if (error)
    {
        return {error, 400};
    }

    return {SUCCESS, 200};
}

//////////////////
/* PUT REQUESTS */
//////////////////

std::tuple<Error, uint16_t> ApiDispatcher::saveWallet(const Request &req, Response &res,
                                                      const nlohmann::json &body) const
{
    std::scoped_lock lock(m_mutex);

    m_walletBackend->save();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::resetWallet(const Request &req, Response &res, const nlohmann::json &body)
{
    std::scoped_lock lock(m_mutex);

    uint64_t scanHeight = 0;
    uint64_t timestamp = 0;

    if (body.find("scanHeight") != body.end())
    {
        scanHeight = tryGetJsonValue<uint64_t>(body, "scanHeight");
    }

    m_walletBackend->reset(scanHeight, timestamp);

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::setNodeInfo(const Request &req, Response &res, const nlohmann::json &body)
{
    std::scoped_lock lock(m_mutex);

    const std::string daemonHost = tryGetJsonValue<std::string>(body, "daemonHost");
    const uint16_t daemonPort = tryGetJsonValue<uint16_t>(body, "daemonPort");

    m_walletBackend->swapNode(daemonHost, daemonPort);

    return {SUCCESS, 202};
}

//////////////////
/* GET REQUESTS */
//////////////////

std::tuple<Error, uint16_t> ApiDispatcher::getNodeInfo(const Request &req, Response &res,
                                                       const nlohmann::json &body) const
{
    const auto [daemonHost, daemonPort] = m_walletBackend->getNodeAddress();

    const auto [nodeFee, nodeAddress] = m_walletBackend->getNodeFee();

    nlohmann::json j{
        {"daemonHost", daemonHost}, {"daemonPort", daemonPort}, {"nodeFee", nodeFee}, {"nodeAddress", nodeAddress}};

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getPrivateViewKey(const Request &req, Response &res,
                                                             const nlohmann::json &body) const
{
    nlohmann::json j{{"privateViewKey", m_walletBackend->getPrivateViewKey()}};

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 200};
}

/* Gets the spend keys for the given address */
std::tuple<Error, uint16_t> ApiDispatcher::getSpendKeys(const Request &req, Response &res,
                                                        const nlohmann::json &body) const
{
    /* Remove the keys prefix to get the address */
    std::string address = req.path.substr(std::string("/keys/").size());

    if (Error error = validateAddresses({address}, false); error != SUCCESS)
    {
        return {error, 400};
    }

    const auto [error, publicSpendKey, privateSpendKey] = m_walletBackend->getSpendKeys(address);

    if (error)
    {
        return {error, 400};
    }

    nlohmann::json j{{"publicSpendKey", publicSpendKey}, {"privateSpendKey", privateSpendKey}};

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 200};
}

/* Gets the mnemonic seed for the given address (if possible) */
std::tuple<Error, uint16_t> ApiDispatcher::getMnemonicSeed(const Request &req, Response &res,
                                                           const nlohmann::json &body) const
{
    /* Remove the keys prefix to get the address */
    std::string address = req.path.substr(std::string("/keys/mnemonic/").size());

    if (Error error = validateAddresses({address}, false); error != SUCCESS)
    {
        return {error, 400};
    }

    const auto [error, mnemonicSeed] = m_walletBackend->getMnemonicSeedForAddress(address);

    if (error)
    {
        return {error, 400};
    }

    nlohmann::json j{{"mnemonicSeed", mnemonicSeed}};

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getStatus(const Request &req, Response &res,
                                                     const nlohmann::json &body) const
{
    const WalletTypes::WalletStatus status = m_walletBackend->getStatus();

    nlohmann::json j{{"walletBlockCount", status.walletBlockCount},
                     {"localDaemonBlockCount", status.localDaemonBlockCount},
                     {"networkBlockCount", status.networkBlockCount},
                     {"peerCount", status.peerCount},
                     {"hashrate", status.lastKnownHashrate},
                     {"isViewWallet", m_walletBackend->isViewWallet()},
                     {"subWalletCount", m_walletBackend->getWalletCount()}};

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getAddresses(const Request &req, Response &res,
                                                        const nlohmann::json &body) const
{
    nlohmann::json j{{"addresses", m_walletBackend->getAddresses()}};

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getPrimaryAddress(const Request &req, Response &res,
                                                             const nlohmann::json &body) const
{
    nlohmann::json j{{"address", m_walletBackend->getPrimaryAddress()}};

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::createIntegratedAddress(const Request &req, Response &res,
                                                                   const nlohmann::json &body) const
{
    std::string stripped = req.path.substr(std::string("/addresses/").size());

    uint64_t splitPos = stripped.find_first_of("/");

    std::string address = stripped.substr(0, splitPos);

    /* Skip the address */
    std::string paymentID = stripped.substr(splitPos + 1);

    const auto [error, integratedAddress] = WalletBackend::createIntegratedAddress(address, paymentID);

    if (error)
    {
        return {error, 400};
    }

    nlohmann::json j{{"integratedAddress", integratedAddress}};

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getTransactions(const Request &req, Response &res,
                                                           const nlohmann::json &body) const
{
    nlohmann::json j{{"transactions", m_walletBackend->getTransactions()}};

    publicKeysToAddresses(j);

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getUnconfirmedTransactions(const Request &req, Response &res,
                                                                      const nlohmann::json &body) const
{
    nlohmann::json j{{"transactions", m_walletBackend->getUnconfirmedTransactions()}};

    publicKeysToAddresses(j);

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getUnconfirmedTransactionsForAddress(const Request &req, Response &res,
                                                                                const nlohmann::json &body) const
{
    std::string address = req.path.substr(std::string("/transactions/unconfirmed").size());

    const auto txs = m_walletBackend->getUnconfirmedTransactions();

    std::vector<WalletTypes::Transaction> result;

    std::copy_if(txs.begin(), txs.end(), std::back_inserter(result), [address, this](const auto tx) {
        for (const auto [key, transfer] : tx.transfers)
        {
            const auto [error, actualAddress] = m_walletBackend->getAddress(key);

            /* If the transfer contains our address, keep it, else skip */
            if (actualAddress == address)
            {
                return true;
            }
        }

        return false;
    });

    nlohmann::json j{{"transactions", result}};

    publicKeysToAddresses(j);

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getTransactionsFromHeight(const httplib::Request &req,
                                                                     httplib::Response &res,
                                                                     const nlohmann::json &body) const
{
    std::string startHeightStr = req.path.substr(std::string("/transactions/").size());

    try
    {
        uint64_t startHeight = std::stoull(startHeightStr);

        const auto txs = m_walletBackend->getTransactionsRange(startHeight, startHeight + 1000);

        nlohmann::json j{{"transactions", txs}};

        publicKeysToAddresses(j);

        res.set_content(j.dump(4) + "\n", "application/json");

        return {SUCCESS, 200};
    }
    catch (const std::out_of_range &)
    {
        std::cout << "Height parameter is too large or too small!" << std::endl;
        return {SUCCESS, 400};
    }
    catch (const std::invalid_argument &e)
    {
        std::cout << "Failed to parse parameter as height: " << e.what() << std::endl;
        return {SUCCESS, 400};
    }
}

std::tuple<Error, uint16_t> ApiDispatcher::getTransactionsFromHeightToHeight(const httplib::Request &req,
                                                                             httplib::Response &res,
                                                                             const nlohmann::json &body) const
{
    std::string stripped = req.path.substr(std::string("/transactions/").size());

    uint64_t splitPos = stripped.find_first_of("/");

    /* Take all the chars before the "/", this is our start height */
    std::string startHeightStr = stripped.substr(0, splitPos);

    /* Take all the chars after the "/", this is our end height */
    std::string endHeightStr = stripped.substr(splitPos + 1);

    try
    {
        uint64_t startHeight = std::stoull(startHeightStr);

        uint64_t endHeight = std::stoull(endHeightStr);

        if (startHeight >= endHeight)
        {
            std::cout << "Start height must be < end height..." << std::endl;
            return {SUCCESS, 400};
        }

        const auto txs = m_walletBackend->getTransactionsRange(startHeight, endHeight);

        nlohmann::json j{{"transactions", txs}};

        publicKeysToAddresses(j);

        res.set_content(j.dump(4) + "\n", "application/json");

        return {SUCCESS, 200};
    }
    catch (const std::out_of_range &)
    {
        std::cout << "Height parameter is too large or too small!" << std::endl;
        return {SUCCESS, 400};
    }
    catch (const std::invalid_argument &)
    {
        std::cout << "Failed to parse parameter as height...\n";
        return {SUCCESS, 400};
    }
}

std::tuple<Error, uint16_t> ApiDispatcher::getTransactionsFromHeightWithAddress(const httplib::Request &req,
                                                                                httplib::Response &res,
                                                                                const nlohmann::json &body) const
{
    std::string stripped = req.path.substr(std::string("/transactions/address/").size());

    uint64_t splitPos = stripped.find_first_of("/");

    std::string address = stripped.substr(0, splitPos);

    if (Error error = validateAddresses({address}, false); error != SUCCESS)
    {
        return {error, 400};
    }

    /* Skip the address */
    std::string startHeightStr = stripped.substr(splitPos + 1);

    try
    {
        uint64_t startHeight = std::stoull(startHeightStr);

        const auto txs = m_walletBackend->getTransactionsRange(startHeight, startHeight + 1000);

        std::vector<WalletTypes::Transaction> result;

        std::copy_if(txs.begin(), txs.end(), std::back_inserter(result), [address, this](const auto tx) {
            for (const auto [key, transfer] : tx.transfers)
            {
                const auto [error, actualAddress] = m_walletBackend->getAddress(key);

                /* If the transfer contains our address, keep it, else skip */
                if (actualAddress == address)
                {
                    return true;
                }
            }

            return false;
        });

        nlohmann::json j{{"transactions", result}};

        publicKeysToAddresses(j);

        res.set_content(j.dump(4) + "\n", "application/json");

        return {SUCCESS, 200};
    }
    catch (const std::out_of_range &)
    {
        std::cout << "Height parameter is too large or too small!" << std::endl;
        return {SUCCESS, 400};
    }
    catch (const std::invalid_argument &)
    {
        std::cout << "Failed to parse parameter as height...\n";
        return {SUCCESS, 400};
    }
}

std::tuple<Error, uint16_t>
ApiDispatcher::getTransactionsFromHeightToHeightWithAddress(const httplib::Request &req, httplib::Response &res,
                                                            const nlohmann::json &body) const
{
    std::string stripped = req.path.substr(std::string("/transactions/address/").size());

    uint64_t splitPos = stripped.find_first_of("/");

    std::string address = stripped.substr(0, splitPos);

    if (Error error = validateAddresses({address}, false); error != SUCCESS)
    {
        return {error, 400};
    }

    stripped = stripped.substr(splitPos + 1);

    splitPos = stripped.find_first_of("/");

    /* Take all the chars before the "/", this is our start height */
    std::string startHeightStr = stripped.substr(0, splitPos);

    /* Take all the chars after the "/", this is our end height */
    std::string endHeightStr = stripped.substr(splitPos + 1);

    try
    {
        uint64_t startHeight = std::stoull(startHeightStr);

        uint64_t endHeight = std::stoull(endHeightStr);

        if (startHeight >= endHeight)
        {
            std::cout << "Start height must be < end height..." << std::endl;
            return {SUCCESS, 400};
        }

        const auto txs = m_walletBackend->getTransactionsRange(startHeight, endHeight);

        std::vector<WalletTypes::Transaction> result;

        std::copy_if(txs.begin(), txs.end(), std::back_inserter(result), [address, this](const auto tx) {
            for (const auto [key, transfer] : tx.transfers)
            {
                const auto [error, actualAddress] = m_walletBackend->getAddress(key);

                /* If the transfer contains our address, keep it, else skip */
                if (actualAddress == address)
                {
                    return true;
                }
            }

            return false;
        });

        nlohmann::json j{{"transactions", result}};

        publicKeysToAddresses(j);

        res.set_content(j.dump(4) + "\n", "application/json");

        return {SUCCESS, 200};
    }
    catch (const std::out_of_range &)
    {
        std::cout << "Height parameter is too large or too small!" << std::endl;
        return {SUCCESS, 400};
    }
    catch (const std::invalid_argument &)
    {
        std::cout << "Failed to parse parameter as height...\n";
        return {SUCCESS, 400};
    }
}

std::tuple<Error, uint16_t> ApiDispatcher::getTransactionDetails(const httplib::Request &req, httplib::Response &res,
                                                                 const nlohmann::json &body) const
{
    std::string hashStr = req.path.substr(std::string("/transactions/hash/").size());

    Crypto::Hash hash;

    Common::podFromHex(hashStr, hash.data);

    for (const auto tx : m_walletBackend->getTransactions())
    {
        if (tx.hash == hash)
        {
            nlohmann::json j{{"transaction", tx}};

            res.set_content(j.dump(4) + "\n", "application/json");

            return {SUCCESS, 200};
        }
    }

    /* Not found */
    return {SUCCESS, 404};
}

std::tuple<Error, uint16_t> ApiDispatcher::getBalance(const httplib::Request &req, httplib::Response &res,
                                                      const nlohmann::json &body) const
{
    const auto [unlocked, locked] = m_walletBackend->getTotalBalance();

    nlohmann::json j{{"unlocked", unlocked}, {"locked", locked}};

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getBalanceForAddress(const httplib::Request &req, httplib::Response &res,
                                                                const nlohmann::json &body) const
{
    std::string address = req.path.substr(std::string("/balance/").size());

    const auto [error, unlocked, locked] = m_walletBackend->getBalance(address);

    if (error)
    {
        return {error, 400};
    }

    nlohmann::json j{{"unlocked", unlocked}, {"locked", locked}};

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getBalances(const httplib::Request &req, httplib::Response &res,
                                                       const nlohmann::json &body) const
{
    const auto balances = m_walletBackend->getBalances();

    nlohmann::json j;

    for (const auto [address, unlocked, locked] : balances)
    {
        j.push_back({{"address", address}, {"unlocked", unlocked}, {"locked", locked}});
    }

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getTxPrivateKey(const httplib::Request &req, httplib::Response &res,
                                                           const nlohmann::json &body) const
{
    std::string txHashStr = req.path.substr(std::string("/transactions/privatekey/").size());

    Crypto::Hash txHash;

    Common::podFromHex(txHashStr, txHash.data);

    const auto [error, key] = m_walletBackend->getTxPrivateKey(txHash);

    if (error)
    {
        return {error, 400};
    }

    nlohmann::json j{{"transactionPrivateKey", key}};

    res.set_content(j.dump(4) + "\n", "application/json");

    return {SUCCESS, 200};
}

//////////////////////
/* OPTIONS REQUESTS */
//////////////////////

void ApiDispatcher::handleOptions(const Request &req, Response &res) const
{
    std::cout << "Incoming " << req.method << " request: " << req.path << std::endl;

    std::string supported = "OPTIONS, GET, POST, PUT, DELETE";

    if (m_corsHeader == "")
    {
        supported = "";
    }

    if (req.has_header("Access-Control-Request-Method"))
    {
        res.set_header("Access-Control-Allow-Methods", supported);
    }
    else
    {
        res.set_header("Allow", supported);
    }

    /* Add the cors header if not empty string */
    if (m_corsHeader != "")
    {
        res.set_header("Access-Control-Allow-Origin", m_corsHeader);
        res.set_header("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept, X-API-KEY");
    }

    res.status = 200;
}

std::tuple<std::string, uint16_t, std::string, std::string>
ApiDispatcher::getDefaultWalletParams(const nlohmann::json body) const
{
    std::string daemonHost = "127.0.0.1";
    uint16_t daemonPort = CryptoNote::RPC_DEFAULT_PORT;

    const std::string filename = tryGetJsonValue<std::string>(body, "filename");
    const std::string password = tryGetJsonValue<std::string>(body, "password");

    if (body.find("daemonHost") != body.end())
    {
        daemonHost = tryGetJsonValue<std::string>(body, "daemonHost");
    }

    if (body.find("daemonPort") != body.end())
    {
        daemonPort = tryGetJsonValue<uint16_t>(body, "daemonPort");
    }

    return {daemonHost, daemonPort, filename, password};
}

//////////////////////////
/* END OF API FUNCTIONS */
//////////////////////////

bool ApiDispatcher::assertIsNotViewWallet() const
{
    if (m_walletBackend->isViewWallet())
    {
        std::cout << "Client requested to perform an operation which requires "
                     "a non view wallet, but wallet is a view wallet"
                  << std::endl;
        return false;
    }

    return true;
}

bool ApiDispatcher::assertIsViewWallet() const
{
    if (!m_walletBackend->isViewWallet())
    {
        std::cout << "Client requested to perform an operation which requires "
                     "a view wallet, but wallet is a non view wallet"
                  << std::endl;
        return false;
    }

    return true;
}

bool ApiDispatcher::assertWalletClosed() const
{
    if (m_walletBackend != nullptr)
    {
        std::cout << "Client requested to open a wallet, whilst one is already open" << std::endl;
        return false;
    }

    return true;
}

bool ApiDispatcher::assertWalletOpen() const
{
    if (m_walletBackend == nullptr)
    {
        std::cout << "Client requested to modify a wallet, whilst no wallet is open" << std::endl;
        return false;
    }

    return true;
}

void ApiDispatcher::publicKeysToAddresses(nlohmann::json &j) const
{
    for (auto &item : j.at("transactions"))
    {
        /* Replace publicKey with address for ease of use */
        for (auto &tx : item.at("transfers"))
        {
            /* Get the spend key */
            Crypto::PublicKey spendKey = tx.at("publicKey").get<Crypto::PublicKey>();

            /* Get the address it belongs to */
            const auto [error, address] = m_walletBackend->getAddress(spendKey);

            /* Add the address to the json */
            tx["address"] = address;

            /* Remove the spend key */
            tx.erase("publicKey");
        }
    }
}

std::string ApiDispatcher::hashPassword(const std::string password) const
{
    using namespace CryptoPP;

    /* Using SHA256 as the algorithm */
    CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> pbkdf2;

    byte key[16];

    /* Hash the password with pbkdf2 */
    pbkdf2.DeriveKey(key, sizeof(key), 0, (byte *)password.c_str(), password.size(), m_salt, sizeof(m_salt),
                     ApiConstants::PBKDF2_ITERATIONS);

    return Common::podToHex(key);
}
