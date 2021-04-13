// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

////////////////////////////////////
#include <walletapi/ApiDispatcher.h>
////////////////////////////////////

#include <common/StringTools.h>
#include <config/CryptoNoteConfig.h>
#include <crypto/random.h>
#include <cryptonotecore/Mixins.h>
#include <cryptopp/modes.h>
#include <cryptopp/pwdbased.h>
#include <cryptopp/sha.h>
#include <errors/ValidateParameters.h>
#include <iomanip>
#include <iostream>
#include <utilities/Addresses.h>
#include <utilities/ColouredMsg.h>
#include <walletapi/Constants.h>

ApiDispatcher::ApiDispatcher(
    const uint16_t bindPort,
    const std::string rpcBindIp,
    const std::string rpcPassword,
    const std::string corsHeader,
    unsigned int walletSyncThreads):
    m_port(bindPort), m_host(rpcBindIp), m_corsHeader(corsHeader), m_rpcPassword(rpcPassword)
{
    if (walletSyncThreads == 0)
    {
        walletSyncThreads = 1;
    }

    m_walletSyncThreads = walletSyncThreads;

    /* Generate the salt used for pbkdf2 api authentication */
    Random::randomBytes(16, m_salt);

    /* Make sure to do this after initializing the salt above! */
    m_hashedPassword = hashPassword(rpcPassword);

    using namespace std::placeholders;

    /* Route the request through our middleware function, before forwarding
       to the specified function */
    const auto router = [this](
                            const auto function,
                            const WalletState walletState,
                            const bool viewWalletPermitted,
                            const bool isBodyRequired) {
        return [=](const httplib::Request &req, httplib::Response &res) {
            /* Pass the inputted function with the arguments passed through
               to middleware */
            middleware(
                req,
                res,
                walletState,
                viewWalletPermitted,
                isBodyRequired,
                std::bind(function, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        };
    };

    const bool bodyRequired = true;

    const bool bodyNotRequired = false;

    const bool viewWalletsAllowed = true;

    const bool viewWalletsBanned = false;

    /* POST */
    m_server
        .Post("/wallet/open", router(&ApiDispatcher::openWallet, WalletMustBeClosed, viewWalletsAllowed, bodyRequired))

        /* Import wallet with keys */
        .Post(
            "/wallet/import/key",
            router(&ApiDispatcher::keyImportWallet, WalletMustBeClosed, viewWalletsAllowed, bodyRequired))

        /* Import wallet with seed */
        .Post(
            "/wallet/import/seed",
            router(&ApiDispatcher::seedImportWallet, WalletMustBeClosed, viewWalletsAllowed, bodyRequired))

        /* Import view wallet */
        .Post(
            "/wallet/import/view",
            router(&ApiDispatcher::importViewWallet, WalletMustBeClosed, viewWalletsAllowed, bodyRequired))

        /* Create wallet */
        .Post(
            "/wallet/create",
            router(&ApiDispatcher::createWallet, WalletMustBeClosed, viewWalletsAllowed, bodyRequired))

        /* Create a random address */
        .Post(
            "/addresses/create",
            router(&ApiDispatcher::createAddress, WalletMustBeOpen, viewWalletsBanned, bodyRequired))

        /* Import an address with a spend secret key */
        .Post(
            "/addresses/import",
            router(&ApiDispatcher::importAddress, WalletMustBeOpen, viewWalletsBanned, bodyRequired))

        /* Import a deterministic address using a wallet index */
        .Post(
            "/addresses/import/deterministic",
            router(&ApiDispatcher::importDeterministicAddress, WalletMustBeOpen, viewWalletsBanned, bodyRequired))

        /* Import a view only address with a public spend key */
        .Post(
            "/addresses/import/view",
            router(&ApiDispatcher::importViewAddress, WalletMustBeOpen, viewWalletsAllowed, bodyRequired))

        /* Validate an address */
        .Post(
            "/addresses/validate",
            router(&ApiDispatcher::validateAddress, DoesntMatter, viewWalletsAllowed, bodyRequired))

        /* Send a previously prepared transaction */
        .Post(
            "/transactions/send/prepared",
            router(&ApiDispatcher::sendPreparedTransaction, WalletMustBeOpen, viewWalletsBanned, bodyRequired))

        /* Prepare a transaction */
        .Post(
            "/transactions/prepare/basic",
            router(&ApiDispatcher::prepareBasicTransaction, WalletMustBeOpen, viewWalletsBanned, bodyRequired))

        /* Send a transaction */
        .Post(
            "/transactions/send/basic",
            router(&ApiDispatcher::sendBasicTransaction, WalletMustBeOpen, viewWalletsBanned, bodyRequired))

        /* Prepare a transaction, more parameters specified */
        .Post(
            "/transactions/prepare/advanced",
            router(&ApiDispatcher::prepareAdvancedTransaction, WalletMustBeOpen, viewWalletsBanned, bodyRequired))

        /* Send a transaction, more parameters specified */
        .Post(
            "/transactions/send/advanced",
            router(&ApiDispatcher::sendAdvancedTransaction, WalletMustBeOpen, viewWalletsBanned, bodyRequired))

        /* Send a fusion transaction */
        .Post(
            "/transactions/send/fusion/basic",
            router(&ApiDispatcher::sendBasicFusionTransaction, WalletMustBeOpen, viewWalletsBanned, bodyRequired))

        /* Send a fusion transaction, more parameters specified */
        .Post(
            "/transactions/send/fusion/advanced",
            router(&ApiDispatcher::sendAdvancedFusionTransaction, WalletMustBeOpen, viewWalletsBanned, bodyRequired))

        .Post("/export/json", router(&ApiDispatcher::exportToJSON, WalletMustBeOpen, viewWalletsAllowed, bodyRequired))

        /* DELETE */

        /* Close the current wallet */
        .Delete("/wallet", router(&ApiDispatcher::closeWallet, WalletMustBeOpen, viewWalletsAllowed, bodyNotRequired))

        /* Delete the given address */
        .Delete(
            "/addresses/" + ApiConstants::addressRegex,
            router(&ApiDispatcher::deleteAddress, WalletMustBeOpen, viewWalletsAllowed, bodyNotRequired))

        /* Delete a previously prepared transaction */
        .Delete(
            "/transactions/prepared/" + ApiConstants::hashRegex,
            router(&ApiDispatcher::deletePreparedTransaction, WalletMustBeOpen, viewWalletsBanned, bodyNotRequired))

        /* PUT */

        /* Save the wallet */
        .Put("/save", router(&ApiDispatcher::saveWallet, WalletMustBeOpen, viewWalletsAllowed, bodyRequired))

        /* Reset the wallet from zero, or given scan height */
        .Put("/reset", router(&ApiDispatcher::resetWallet, WalletMustBeOpen, viewWalletsAllowed, bodyNotRequired))

        /* Rewind the wallet to a specific block height */
        .Put("/rewind", router(&ApiDispatcher::rewindWallet, WalletMustBeOpen, viewWalletsAllowed, bodyRequired))

        /* Swap node details */
        .Put("/node", router(&ApiDispatcher::setNodeInfo, WalletMustBeOpen, viewWalletsAllowed, bodyRequired))

        /* GET */

        /* Get node details */
        .Get("/node", router(&ApiDispatcher::getNodeInfo, WalletMustBeOpen, viewWalletsAllowed, bodyNotRequired))

        /* Get the shared private view key */
        .Get("/keys", router(&ApiDispatcher::getPrivateViewKey, WalletMustBeOpen, viewWalletsAllowed, bodyNotRequired))

        /* Get the spend keys for the given address */
        .Get(
            "/keys/" + ApiConstants::addressRegex,
            router(&ApiDispatcher::getSpendKeys, WalletMustBeOpen, viewWalletsBanned, bodyNotRequired))

        /* Get the mnemonic seed for the given address */
        .Get(
            "/keys/mnemonic/" + ApiConstants::addressRegex,
            router(&ApiDispatcher::getMnemonicSeed, WalletMustBeOpen, viewWalletsBanned, bodyNotRequired))

        /* Get the wallet status */
        .Get("/status", router(&ApiDispatcher::getStatus, WalletMustBeOpen, viewWalletsAllowed, bodyNotRequired))

        /* Get a list of all addresses */
        .Get("/addresses", router(&ApiDispatcher::getAddresses, WalletMustBeOpen, viewWalletsAllowed, bodyNotRequired))

        /* Get the primary address */
        .Get(
            "/addresses/primary",
            router(&ApiDispatcher::getPrimaryAddress, WalletMustBeOpen, viewWalletsAllowed, bodyNotRequired))

        /* Creates an integrated address from the given address and payment ID */
        .Get(
            "/addresses/" + ApiConstants::addressRegex + "/" + ApiConstants::hashRegex,
            router(&ApiDispatcher::createIntegratedAddress, WalletMustBeOpen, viewWalletsAllowed, bodyNotRequired))

        /* Get all transactions */
        .Get(
            "/transactions",
            router(&ApiDispatcher::getTransactions, WalletMustBeOpen, viewWalletsAllowed, bodyNotRequired))

        /* Get all (outgoing) unconfirmed transactions */
        .Get(
            "/transactions/unconfirmed",
            router(&ApiDispatcher::getUnconfirmedTransactions, WalletMustBeOpen, viewWalletsAllowed, bodyNotRequired))

        /* Get all (outgoing) unconfirmed transactions, belonging to the given address */
        .Get(
            "/transactions/unconfirmed/" + ApiConstants::addressRegex,
            router(
                &ApiDispatcher::getUnconfirmedTransactionsForAddress,
                WalletMustBeOpen,
                viewWalletsAllowed,
                bodyNotRequired))

        /* Get the transactions starting at the given block, for 1000 blocks */
        .Get(
            "/transactions/(\\d+)",
            router(&ApiDispatcher::getTransactionsFromHeight, WalletMustBeOpen, viewWalletsAllowed, bodyNotRequired))

        /* Get the transactions starting at the given block, and ending at the given block */
        .Get(
            "/transactions/(\\d+)/(\\d+)",
            router(
                &ApiDispatcher::getTransactionsFromHeightToHeight,
                WalletMustBeOpen,
                viewWalletsAllowed,
                bodyNotRequired))

        /* Get the transactions starting at the given block, for 1000 blocks, belonging to the given address */
        .Get(
            "/transactions/address/" + ApiConstants::addressRegex + "/(\\d+)",
            router(
                &ApiDispatcher::getTransactionsFromHeightWithAddress,
                WalletMustBeOpen,
                viewWalletsAllowed,
                bodyNotRequired))

        /* Get the transactions starting at the given block, and ending at the given block, belonging to the given
           address */
        .Get(
            "/transactions/address/" + ApiConstants::addressRegex + "/(\\d+)" + "/(\\d+)",
            router(
                &ApiDispatcher::getTransactionsFromHeightToHeightWithAddress,
                WalletMustBeOpen,
                viewWalletsAllowed,
                bodyNotRequired))

        /* Get the transaction private key for the given hash */
        .Get(
            "/transactions/privatekey/" + ApiConstants::hashRegex,
            router(&ApiDispatcher::getTxPrivateKey, WalletMustBeOpen, viewWalletsBanned, bodyNotRequired))

        /* Get details for the given transaction hash, if known */
        .Get(
            "/transactions/hash/" + ApiConstants::hashRegex,
            router(&ApiDispatcher::getTransactionDetails, WalletMustBeOpen, viewWalletsAllowed, bodyNotRequired))

        .Get(
            "/transactions/paymentid/" + ApiConstants::hashRegex,
            router(&ApiDispatcher::getTransactionsByPaymentId, WalletMustBeOpen, viewWalletsAllowed, bodyNotRequired))

        .Get(
            "/transactions/paymentid",
            router(&ApiDispatcher::getTransactionsWithPaymentId, WalletMustBeOpen, viewWalletsAllowed, bodyNotRequired))

        /* Get balance for the wallet */
        .Get("/balance", router(&ApiDispatcher::getBalance, WalletMustBeOpen, viewWalletsAllowed, bodyNotRequired))

        /* Get balance for a specific address */
        .Get(
            "/balance/" + ApiConstants::addressRegex,
            router(&ApiDispatcher::getBalanceForAddress, WalletMustBeOpen, viewWalletsAllowed, bodyNotRequired))

        /* Get balances for each address */
        .Get("/balances", router(&ApiDispatcher::getBalances, WalletMustBeOpen, viewWalletsAllowed, bodyNotRequired))

        /* OPTIONS */

        /* Matches everything */
        /* NOTE: Not passing through middleware */
        .Options(".*", [this](auto &req, auto &res) { handleOptions(req, res); });
}

void ApiDispatcher::start()
{
    const auto listenError = m_server.listen(m_host, m_port);

    if (listenError != httplib::SUCCESS)
    {
        std::cout << WarningMsg("Failed to start RPC server: ")
                  << WarningMsg(httplib::detail::getSocketErrorMessage(listenError)) << std::endl;

        exit(1);
    }
}

void ApiDispatcher::stop()
{
    m_server.stop();
}

std::optional<rapidjson::Document>
    ApiDispatcher::getJsonBody(const httplib::Request &req, httplib::Response &res, const bool bodyRequired)
{
    rapidjson::Document jsonBody;

    if (req.body == "")
    {
        /* Some compilers are stupid and can't figure out just `return jsonBody`
         * and we can't construct a std::optional(jsonBody) since the copy
         * constructor is deleted, so we need to std::move */
        return std::optional<rapidjson::Document>(std::move(jsonBody));
    }

    /* Some methods, most notably POST(/block) and POST(/transaction) may
     * have plain-text style bodies that will not parse as JSON without
     * being enclosed in quotes. Some libraries properly enclose the values
     * in quotes while others do not. This permits either forms to work */
    const auto bodyAsJSONString = "\"" + req.body + "\"";

    if (jsonBody.Parse(req.body.c_str()).HasParseError() && jsonBody.Parse(bodyAsJSONString.c_str()).HasParseError())
    {
        std::stringstream stream;

        if (!req.body.empty())
        {
            stream << "Warning: received body is not JSON encoded!\n"
                   << "Key/value parameters are NOT supported.\n"
                   << "Body:\n"
                   << req.body;

            Logger::logger.log(stream.str(), Logger::INFO, {Logger::DAEMON_RPC});
        }

        stream << "Failed to parse request body as JSON";

        failRequest(Error(API_INVALID_ARGUMENT, stream.str()), res);

        res.status = 400;

        return std::nullopt;
    }

    return std::optional<rapidjson::Document>(std::move(jsonBody));
}

void ApiDispatcher::middleware(
    const httplib::Request &req,
    httplib::Response &res,
    const WalletState walletState,
    const bool viewWalletPermitted,
    const bool bodyRequired,
    std::function<std::tuple<Error, uint16_t>(
        const httplib::Request &req,
        httplib::Response &res,
        const rapidjson::Document &body)> handler)
{
    std::cout << "Incoming " << req.method << " request: " << req.path << std::endl;

    /* Add the cors header if not empty string */
    if (m_corsHeader != "")
    {
        res.set_header("Access-Control-Allow-Origin", m_corsHeader);
    }

    res.set_header("Content-Type", "application/json");

    if (!checkAuthenticated(req, res))
    {
        failRequest(Error(WRONG_PASSWORD, "The wrong password for the service was provided."), res);

        res.status = 401;

        return;
    }

    /* Wallet must be open for this operation, and it is not */
    if (walletState == WalletMustBeOpen && !assertWalletOpen())
    {
        failRequest(Error(SUCCESS, "Wallet file must be open."), res);

        res.status = 403;

        return;
    }
    /* Wallet must not be open for this operation, and it is */
    else if (walletState == WalletMustBeClosed && !assertWalletClosed())
    {
        failRequest(Error(SUCCESS, "Wallet file must not be open."), res);

        res.status = 403;

        return;
    }

    /* We have a wallet open, view wallets are not permitted, and the wallet is
       a view wallet (wew!) */
    if (m_walletBackend != nullptr && !viewWalletPermitted && !assertIsNotViewWallet())
    {
        failRequest(Error(ILLEGAL_VIEW_WALLET_OPERATION, "Operation cannot be completed on a view-only wallet."), res);

        /* Bad request */
        res.status = 400;

        return;
    }

    const auto jsonBody = getJsonBody(req, res, bodyRequired);

    if (!jsonBody && bodyRequired)
    {
        failRequest(Error(API_BODY_REQUIRED), res);

        res.status = 400;

        return;
    }

    try
    {
        const auto [error, statusCode] = handler(req, res, *jsonBody);

        if (error)
        {
            failRequest(error, res);
        }

        res.status = statusCode;

        return;
    }
    /* Most likely a key was missing. Do the error handling here to make the
       rest of the code simpler */
    catch (const std::invalid_argument &e)
    {
        std::cout << "Caught JSON exception, likely missing required "
                     "json parameter: "
                  << e.what() << std::endl;

        failRequest(Error(API_INVALID_ARGUMENT, e.what()), res);

        res.status = 400;
    }
    catch (const std::exception &e)
    {
        std::cout << "Caught unexpected exception: " << e.what() << std::endl;

        failRequest(Error(API_INTERNAL_ERROR, e.what()), res);

        res.status = 500;
    }
}

void ApiDispatcher::failRequest(const Error error, httplib::Response &res)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("error");
        writer.StartObject();
        {
            writer.Key("code");
            writer.Uint(error.getErrorCode());

            writer.Key("message");
            writer.String(error.getErrorMessage());
        }
        writer.EndObject();
    }
    writer.EndObject();

    res.body = sb.GetString();
}

bool ApiDispatcher::checkAuthenticated(const httplib::Request &req, httplib::Response &res) const
{
    if (!req.has_header("X-API-KEY"))
    {
        std::cout << "Rejecting unauthorized request: X-API-KEY header is missing.\n";

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

    return false;
}

///////////////////
/* POST REQUESTS */
///////////////////

std::tuple<Error, uint16_t>
    ApiDispatcher::openWallet(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    std::scoped_lock lock(m_mutex);

    const auto [daemonHost, daemonPort, daemonSSL, filename, password] = getDefaultWalletParams(body);

    Error error;

    std::tie(error, m_walletBackend) =
        WalletBackend::openWallet(filename, password, daemonHost, daemonPort, daemonSSL, m_walletSyncThreads);

    return {error, 200};
}

std::tuple<Error, uint16_t>
    ApiDispatcher::keyImportWallet(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    std::scoped_lock lock(m_mutex);

    const auto [daemonHost, daemonPort, daemonSSL, filename, password] = getDefaultWalletParams(body);

    Crypto::SecretKey privateViewKey;

    privateViewKey.fromJSON(body, "privateViewKey");

    Crypto::SecretKey privateSpendKey;

    privateSpendKey.fromJSON(body, "privateSpendKey");

    uint64_t scanHeight = 0;

    if (hasMember(body, "scanHeight"))
    {
        scanHeight = getUint64FromJSON(body, "scanHeight");
    }

    Error error;

    std::tie(error, m_walletBackend) = WalletBackend::importWalletFromKeys(
        privateSpendKey,
        privateViewKey,
        filename,
        password,
        scanHeight,
        daemonHost,
        daemonPort,
        daemonSSL,
        m_walletSyncThreads);

    return {error, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::seedImportWallet(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    std::scoped_lock lock(m_mutex);

    const auto [daemonHost, daemonPort, daemonSSL, filename, password] = getDefaultWalletParams(body);

    const std::string mnemonicSeed = getStringFromJSON(body, "mnemonicSeed");

    uint64_t scanHeight = 0;

    if (hasMember(body, "scanHeight"))
    {
        scanHeight = getUint64FromJSON(body, "scanHeight");
    }

    Error error;

    std::tie(error, m_walletBackend) = WalletBackend::importWalletFromSeed(
        mnemonicSeed, filename, password, scanHeight, daemonHost, daemonPort, daemonSSL, m_walletSyncThreads);

    return {error, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::importViewWallet(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    std::scoped_lock lock(m_mutex);

    const auto [daemonHost, daemonPort, daemonSSL, filename, password] = getDefaultWalletParams(body);

    const std::string address = getStringFromJSON(body, "address");

    Crypto::SecretKey privateViewKey;

    privateViewKey.fromJSON(body, "privateViewKey");

    uint64_t scanHeight = 0;

    if (hasMember(body, "scanHeight"))
    {
        scanHeight = getUint64FromJSON(body, "scanHeight");
    }

    Error error;

    std::tie(error, m_walletBackend) = WalletBackend::importViewWallet(
        privateViewKey,
        address,
        filename,
        password,
        scanHeight,
        daemonHost,
        daemonPort,
        daemonSSL,
        m_walletSyncThreads);

    return {error, 200};
}

std::tuple<Error, uint16_t>
    ApiDispatcher::createWallet(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    std::scoped_lock lock(m_mutex);

    const auto [daemonHost, daemonPort, daemonSSL, filename, password] = getDefaultWalletParams(body);

    Error error;

    std::tie(error, m_walletBackend) =
        WalletBackend::createWallet(filename, password, daemonHost, daemonPort, daemonSSL, m_walletSyncThreads);

    return {error, 200};
}

std::tuple<Error, uint16_t>
    ApiDispatcher::createAddress(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    const auto [error, address, privateSpendKey, subWalletIndex] = m_walletBackend->addSubWallet();

    const auto [publicSpendKey, publicViewKey] = Utilities::addressToKeys(address);

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("address");
        writer.String(address);

        writer.Key("privateSpendKey");
        privateSpendKey.toJSON(writer);

        writer.Key("publicSpendKey");
        publicSpendKey.toJSON(writer);

        writer.Key("walletIndex");
        writer.Uint64(subWalletIndex);
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 201};
}

std::tuple<Error, uint16_t>
    ApiDispatcher::importAddress(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    uint64_t scanHeight = 0;

    /* Strongly suggested to supply a scan height. Wallet syncing will have to
       begin again from zero if none is given */
    if (hasMember(body, "scanHeight"))
    {
        scanHeight = getUint64FromJSON(body, "scanHeight");
    }

    Crypto::SecretKey privateSpendKey;

    privateSpendKey.fromJSON(body, "privateSpendKey");

    const auto [error, address] = m_walletBackend->importSubWallet(privateSpendKey, scanHeight);

    if (error)
    {
        return {error, 400};
    }

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("address");
        writer.String(address);
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 201};
}

std::tuple<Error, uint16_t> ApiDispatcher::importDeterministicAddress(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    uint64_t scanHeight = 0;

    /* Strongly suggested to supply a scan height. Wallet syncing will have to
       begin again from zero if none is given */
    if (hasMember(body, "scanHeight"))
    {
        scanHeight = getUint64FromJSON(body, "scanHeight");
    }

    const auto walletIndex = getUint64FromJSON(body, "walletIndex");

    const auto [error, address] = m_walletBackend->importSubWallet(walletIndex, scanHeight);

    if (error)
    {
        return {error, 400};
    }

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("address");
        writer.String(address);
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 201};
}

std::tuple<Error, uint16_t> ApiDispatcher::importViewAddress(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    uint64_t scanHeight = 0;

    /* Strongly suggested to supply a scan height. Wallet syncing will have to
       begin again from zero if none is given */
    if (hasMember(body, "scanHeight"))
    {
        scanHeight = getUint64FromJSON(body, "scanHeight");
    }

    Crypto::PublicKey publicSpendKey;

    publicSpendKey.fromJSON(body, "publicSpendKey");

    const auto [error, address] = m_walletBackend->importViewSubWallet(publicSpendKey, scanHeight);

    if (error)
    {
        return {error, 400};
    }

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("address");
        writer.String(address);
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 201};
}

std::tuple<Error, uint16_t>
    ApiDispatcher::validateAddress(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    const std::string address = getStringFromJSON(body, "address");

    const Error error = validateAddresses({address}, true);

    if (error != SUCCESS)
    {
        return {error, 400};
    }

    std::string actualAddress = address;

    std::string paymentID = "";

    const bool isIntegrated = address.length() == WalletConfig::integratedAddressLength;

    if (isIntegrated)
    {
        std::tie(actualAddress, paymentID) = Utilities::extractIntegratedAddressData(address);
    }

    const auto [publicSpendKey, publicViewKey] = Utilities::addressToKeys(actualAddress);

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("isIntegrated");
        writer.Bool(address.length() == WalletConfig::integratedAddressLength);

        writer.Key("paymentID");
        writer.String(paymentID);

        writer.Key("actualAddress");
        writer.String(actualAddress);

        writer.Key("publicSpendKey");
        publicSpendKey.toJSON(writer);

        writer.Key("publicViewKey");
        publicViewKey.toJSON(writer);
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::sendPreparedTransaction(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    Crypto::Hash hash;

    hash.fromJSON(body, "transactionHash");

    auto [error, hashResult] = m_walletBackend->sendPreparedTransaction(hash);

    if (error)
    {
        return {error, 400};
    }

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("transactionHash");
        hashResult.toJSON(writer);
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 201};
}

std::tuple<Error, uint16_t> ApiDispatcher::prepareBasicTransaction(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    return makeBasicTransaction(req, res, body, false);
}

std::tuple<Error, uint16_t> ApiDispatcher::sendBasicTransaction(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    return makeBasicTransaction(req, res, body, true);
}

std::tuple<Error, uint16_t> ApiDispatcher::makeBasicTransaction(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body,
    const bool sendTransaction)
{
    const std::string address = getStringFromJSON(body, "destination");

    const uint64_t amount = getUint64FromJSON(body, "amount");

    std::string paymentID;

    if (hasMember(body, "paymentID"))
    {
        paymentID = getStringFromJSON(body, "paymentID");
    }

    auto [error, hash, preparedTransaction] = m_walletBackend->sendTransactionBasic(
        address,
        amount,
        paymentID,
        false, /* Don't send all */
        sendTransaction);

    if (error)
    {
        return {error, 400};
    }

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("transactionHash");
        hash.toJSON(writer);

        writer.Key("fee");
        writer.Uint64(preparedTransaction.fee);

        writer.Key("relayedToNetwork");
        writer.Bool(sendTransaction);
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 201};
}

std::tuple<Error, uint16_t> ApiDispatcher::prepareAdvancedTransaction(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    return makeAdvancedTransaction(req, res, body, false);
}

std::tuple<Error, uint16_t> ApiDispatcher::sendAdvancedTransaction(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    return makeAdvancedTransaction(req, res, body, true);
}

std::tuple<Error, uint16_t> ApiDispatcher::makeAdvancedTransaction(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body,
    const bool sendTransaction)
{
    std::vector<std::pair<std::string, uint64_t>> destinations;

    for (const auto &destination : getArrayFromJSON(body, "destinations"))
    {
        const std::string address = getStringFromJSON(destination, "address");

        const uint64_t amount = getUint64FromJSON(destination, "amount");

        destinations.emplace_back(address, amount);
    }

    uint64_t mixin;

    if (hasMember(body, "mixin"))
    {
        mixin = getUint64FromJSON(body, "mixin");
    }
    else
    {
        /* Get the default mixin */
        std::tie(std::ignore, std::ignore, mixin) =
            Utilities::getMixinAllowableRange(m_walletBackend->getStatus().networkBlockCount);
    }

    auto fee = WalletTypes::FeeType::MinimumFee();

    if (hasMember(body, "fee"))
    {
        fee = WalletTypes::FeeType::FixedFee(getUint64FromJSON(body, "fee"));
    }
    else if (hasMember(body, "feePerByte"))
    {
        fee = WalletTypes::FeeType::FeePerByte(getDoubleFromJSON(body, "feePerByte"));
    }

    std::vector<std::string> subWalletsToTakeFrom = {};

    if (hasMember(body, "sourceAddresses"))
    {
        for (const auto &source : getArrayFromJSON(body, "sourceAddresses"))
        {
            subWalletsToTakeFrom.push_back(getStringFromJSON(source));
        }
    }

    std::string paymentID;

    if (hasMember(body, "paymentID"))
    {
        paymentID = getStringFromJSON(body, "paymentID");
    }

    std::string changeAddress;

    if (hasMember(body, "changeAddress"))
    {
        changeAddress = getStringFromJSON(body, "changeAddress");
    }

    uint64_t unlockTime = 0;

    if (hasMember(body, "unlockTime"))
    {
        unlockTime = getUint64FromJSON(body, "unlockTime");
    }

    std::vector<uint8_t> extraData;

    if (hasMember(body, "extra"))
    {
        std::string extra = getStringFromJSON(body, "extra");

        if (!Common::fromHex(extra, extraData))
        {
            return {INVALID_EXTRA_DATA, 400};
        }
    }

    auto [error, hash, preparedTransaction] = m_walletBackend->sendTransactionAdvanced(
        destinations,
        mixin,
        fee,
        paymentID,
        subWalletsToTakeFrom,
        changeAddress,
        unlockTime,
        extraData,
        false, /* Don't send all */
        sendTransaction);

    if (error)
    {
        return {error, 400};
    }

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("transactionHash");
        hash.toJSON(writer);

        writer.Key("fee");
        writer.Uint64(preparedTransaction.fee);

        writer.Key("relayedToNetwork");
        writer.Bool(sendTransaction);
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 201};
}

std::tuple<Error, uint16_t> ApiDispatcher::sendBasicFusionTransaction(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    auto [error, hash] = m_walletBackend->sendFusionTransactionBasic();

    if (error)
    {
        return {error, 400};
    }

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("transactionHash");
        hash.toJSON(writer);
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 201};
}

std::tuple<Error, uint16_t> ApiDispatcher::sendAdvancedFusionTransaction(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    std::string destination;

    if (hasMember(body, "destination"))
    {
        destination = getStringFromJSON(body, "destination");
    }
    else
    {
        destination = m_walletBackend->getPrimaryAddress();
    }

    uint64_t mixin;

    if (hasMember(body, "mixin"))
    {
        mixin = getUint64FromJSON(body, "mixin");
    }
    else
    {
        /* Get the default mixin */
        std::tie(std::ignore, std::ignore, mixin) =
            Utilities::getMixinAllowableRange(m_walletBackend->getStatus().networkBlockCount);
    }

    std::vector<std::string> subWalletsToTakeFrom = {};

    if (hasMember(body, "sourceAddresses"))
    {
        for (const auto &source : getArrayFromJSON(body, "sourceAddresses"))
        {
            subWalletsToTakeFrom.push_back(getStringFromJSON(source));
        }
    }

    std::vector<uint8_t> extraData;

    if (hasMember(body, "extra"))
    {
        std::string extra = getStringFromJSON(body, "extra");

        if (!Common::fromHex(extra, extraData))
        {
            return {INVALID_EXTRA_DATA, 400};
        }
    }

    std::optional<uint64_t> optimizeTarget;

    if (hasMember(body, "optimizeTarget"))
    {
        *optimizeTarget = getUint64FromJSON(body, "optimizeTarget");
    }

    auto [error, hash] = m_walletBackend->sendFusionTransactionAdvanced(
        mixin, subWalletsToTakeFrom, destination, extraData, optimizeTarget);

    if (error)
    {
        return {error, 400};
    }

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("transactionHash");
        hash.toJSON(writer);
    }

    res.body = sb.GetString();

    return {SUCCESS, 201};
}

std::tuple<Error, uint16_t>
    ApiDispatcher::exportToJSON(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    const std::string path = getStringFromJSON(body, "filename");

    const std::string walletJSON = m_walletBackend->toJSON();

    std::ofstream file(path);

    if (!file)
    {
        const Error error = Error(
            INVALID_WALLET_FILENAME, std::string("Could not create file at path given. Error: ") + strerror(errno));

        return {error, 400};
    }

    file << walletJSON << std::endl;

    return {SUCCESS, 200};
}

/////////////////////
/* DELETE REQUESTS */
/////////////////////

std::tuple<Error, uint16_t>
    ApiDispatcher::closeWallet(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    std::scoped_lock lock(m_mutex);

    m_walletBackend = nullptr;

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t>
    ApiDispatcher::deleteAddress(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    /* Remove the addresses prefix to get the address */
    std::string address = req.matches[1];

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

std::tuple<Error, uint16_t> ApiDispatcher::deletePreparedTransaction(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    /* Remove the path prefix to get the hash */
    std::string hashStr = req.matches[1];

    Crypto::Hash hash;

    Common::podFromHex(hashStr, hash.data);

    const bool removed = m_walletBackend->removePreparedTransaction(hash);

    if (removed)
    {
        return {SUCCESS, 200};
    }
    else
    {
        return {SUCCESS, 404};
    }
}

//////////////////
/* PUT REQUESTS */
//////////////////

std::tuple<Error, uint16_t> ApiDispatcher::saveWallet(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    std::scoped_lock lock(m_mutex);

    m_walletBackend->save();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t>
    ApiDispatcher::resetWallet(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    std::scoped_lock lock(m_mutex);

    uint64_t scanHeight = 0;

    uint64_t timestamp = 0;

    if (hasMember(body, "scanHeight"))
    {
        scanHeight = getUint64FromJSON(body, "scanHeight");
    }

    m_walletBackend->reset(scanHeight, timestamp);

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t>
    ApiDispatcher::rewindWallet(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    std::scoped_lock lock(m_mutex);

    uint64_t scanHeight = 0;

    uint64_t timestamp = 0;

    if (hasMember(body, "scanHeight"))
    {
        scanHeight = getUint64FromJSON(body, "scanHeight");
    }

    m_walletBackend->rewind(scanHeight, timestamp);

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t>
    ApiDispatcher::setNodeInfo(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    std::scoped_lock lock(m_mutex);

    uint16_t daemonPort = CryptoNote::RPC_DEFAULT_PORT;
    bool daemonSSL = false;

    /* This parameter is required */
    const std::string daemonHost = getStringFromJSON(body, "daemonHost");

    /* These parameters are optional */
    if (hasMember(body, "daemonPort"))
    {
        daemonPort = getUintFromJSON(body, "daemonPort");
    }

    if (hasMember(body, "daemonSSL"))
    {
        daemonSSL = getBoolFromJSON(body, "daemonSSL");
    }

    m_walletBackend->swapNode(daemonHost, daemonPort, daemonSSL);

    return {SUCCESS, 200};
}

//////////////////
/* GET REQUESTS */
//////////////////

std::tuple<Error, uint16_t> ApiDispatcher::getNodeInfo(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    const auto [daemonHost, daemonPort, daemonSSL] = m_walletBackend->getNodeAddress();

    const auto [nodeFee, nodeAddress] = m_walletBackend->getNodeFee();

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("daemonHost");
        writer.String(daemonHost);

        writer.Key("daemonPort");
        writer.Uint(daemonPort);

        writer.Key("daemonSSL");
        writer.Bool(daemonSSL);

        writer.Key("nodeFee");
        writer.Uint64(nodeFee);

        writer.Key("nodeAddress");
        writer.String(nodeAddress);
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getPrivateViewKey(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("privateViewKey");
        m_walletBackend->getPrivateViewKey().toJSON(writer);
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

/* Gets the spend keys for the given address */
std::tuple<Error, uint16_t> ApiDispatcher::getSpendKeys(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    /* Remove the keys prefix to get the address */
    std::string address = req.matches[1];

    if (Error error = validateAddresses({address}, false); error != SUCCESS)
    {
        return {error, 400};
    }

    const auto [error, publicSpendKey, privateSpendKey, walletIndex] = m_walletBackend->getSpendKeys(address);

    if (error)
    {
        return {error, 400};
    }

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("publicSpendKey");
        publicSpendKey.toJSON(writer);

        writer.Key("privateSpendKey");
        privateSpendKey.toJSON(writer);

        writer.Key("walletIndex");
        writer.Uint64(walletIndex);
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

/* Gets the mnemonic seed for the given address (if possible) */
std::tuple<Error, uint16_t> ApiDispatcher::getMnemonicSeed(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    /* Remove the keys prefix to get the address */
    std::string address = req.matches[1];

    if (Error error = validateAddresses({address}, false); error != SUCCESS)
    {
        return {error, 400};
    }

    const auto [error, mnemonicSeed] = m_walletBackend->getMnemonicSeedForAddress(address);

    if (error)
    {
        return {error, 400};
    }

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("mnemonicSeed");
        writer.String(mnemonicSeed);
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t>
    ApiDispatcher::getStatus(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body) const
{
    const WalletTypes::WalletStatus status = m_walletBackend->getStatus();

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("walletBlockCount");
        writer.Uint64(status.walletBlockCount);

        writer.Key("localDaemonBlockCount");
        writer.Uint64(status.localDaemonBlockCount);

        writer.Key("networkBlockCount");
        writer.Uint64(status.networkBlockCount);

        writer.Key("peerCount");
        writer.Uint64(status.peerCount);

        writer.Key("hashrate");
        writer.Uint64(status.lastKnownHashrate);

        writer.Key("isViewWallet");
        writer.Bool(m_walletBackend->isViewWallet());

        writer.Key("subWalletCount");
        writer.Uint64(m_walletBackend->getWalletCount());
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getAddresses(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("addresses");
        writer.StartArray();
        {
            for (const auto &address : m_walletBackend->getAddresses())
            {
                writer.String(address);
            }
        }
        writer.EndArray();
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getPrimaryAddress(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("address");
        writer.String(m_walletBackend->getPrimaryAddress());
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::createIntegratedAddress(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    std::string address = req.matches[1];

    /* Skip the address */
    std::string paymentID = req.matches[2];

    const auto [error, integratedAddress] = Utilities::createIntegratedAddress(address, paymentID);

    if (error)
    {
        return {error, 400};
    }

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("integratedAddress");
        writer.String(integratedAddress);
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getTransactions(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("transactions");
        writer.StartArray();
        {
            for (const auto &tx : m_walletBackend->getTransactions())
            {
                publicKeysToAddresses(tx, writer);
            }
        }
        writer.EndArray();
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getUnconfirmedTransactions(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("transactions");
        writer.StartArray();
        {
            for (const auto &tx : m_walletBackend->getUnconfirmedTransactions())
            {
                publicKeysToAddresses(tx, writer);
            }
        }
        writer.EndArray();
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getUnconfirmedTransactionsForAddress(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    std::string address = req.matches[1];

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

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("transactions");
        writer.StartArray();
        {
            for (const auto &tx : result)
            {
                publicKeysToAddresses(tx, writer);
            }
        }
        writer.EndArray();
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getTransactionsFromHeight(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    std::string startHeightStr = req.matches[1];

    uint64_t startHeight;

    try
    {
        startHeight = std::stoull(startHeightStr);
    }
    catch (const std::out_of_range &)
    {
        std::cout << "Height parameter is too large or too small!" << std::endl;

        return {Error(API_INVALID_ARGUMENT, "Height parameter is too large or too small."), 400};
    }
    catch (const std::invalid_argument &e)
    {
        std::cout << "Failed to parse parameter as height: " << e.what() << std::endl;

        return {Error(API_INVALID_ARGUMENT, "Height parameter is too large or too small."), 400};
    }

    const auto txs = m_walletBackend->getTransactionsRange(startHeight, startHeight + 1000);

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("transactions");
        writer.StartArray();
        {
            for (const auto &tx : txs)
            {
                publicKeysToAddresses(tx, writer);
            }
        }
        writer.EndArray();
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getTransactionsFromHeightToHeight(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    std::string startHeightStr = req.matches[1];

    std::string endHeightStr = req.matches[2];

    uint64_t startHeight;

    uint64_t endHeight;

    try
    {
        startHeight = std::stoull(startHeightStr);

        endHeight = std::stoull(endHeightStr);
    }
    catch (const std::out_of_range &)
    {
        std::cout << "Height parameter is too large or too small!" << std::endl;

        return {Error(API_INVALID_ARGUMENT, "Height parameter is too large or too small."), 400};
    }
    catch (const std::invalid_argument &e)
    {
        std::cout << "Failed to parse parameter as height: " << e.what() << std::endl;

        return {Error(API_INVALID_ARGUMENT, "Height parameter is too large or too small."), 400};
    }

    if (startHeight >= endHeight)
    {
        std::cout << "Start height must be < end height..." << std::endl;

        return {Error(API_INVALID_ARGUMENT, "Start height must be less than end height."), 400};
    }

    const auto txs = m_walletBackend->getTransactionsRange(startHeight, endHeight);

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("transactions");
        writer.StartArray();
        {
            for (const auto &tx : txs)
            {
                publicKeysToAddresses(tx, writer);
            }
        }
        writer.EndArray();
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getTransactionsFromHeightWithAddress(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    std::string address = req.matches[1];

    if (Error error = validateAddresses({address}, false); error != SUCCESS)
    {
        return {error, 400};
    }

    /* Skip the address */
    std::string startHeightStr = req.matches[2];


    uint64_t startHeight;

    try
    {
        startHeight = std::stoull(startHeightStr);
    }
    catch (const std::out_of_range &)
    {
        std::cout << "Height parameter is too large or too small!" << std::endl;

        return {Error(API_INVALID_ARGUMENT, "Height parameter is too large or too small."), 400};
    }
    catch (const std::invalid_argument &e)
    {
        std::cout << "Failed to parse parameter as height: " << e.what() << std::endl;

        return {Error(API_INVALID_ARGUMENT, "Height parameter is too large or too small."), 400};
    }

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

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("transactions");
        writer.StartArray();
        {
            for (const auto &tx : result)
            {
                publicKeysToAddresses(tx, writer);
            }
        }
        writer.EndArray();
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getTransactionsFromHeightToHeightWithAddress(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    std::string address = req.matches[1];

    if (Error error = validateAddresses({address}, false); error != SUCCESS)
    {
        return {error, 400};
    }

    std::string startHeightStr = req.matches[2];

    std::string endHeightStr = req.matches[3];


    uint64_t startHeight;

    uint64_t endHeight;

    try
    {
        startHeight = std::stoull(startHeightStr);

        endHeight = std::stoull(endHeightStr);
    }
    catch (const std::out_of_range &)
    {
        std::cout << "Height parameter is too large or too small!" << std::endl;

        return {Error(API_INVALID_ARGUMENT, "Height parameter is too large or too small."), 400};
    }
    catch (const std::invalid_argument &e)
    {
        std::cout << "Failed to parse parameter as height: " << e.what() << std::endl;

        return {Error(API_INVALID_ARGUMENT, "Height parameter is too large or too small."), 400};
    }

    if (startHeight >= endHeight)
    {
        std::cout << "Start height must be < end height..." << std::endl;

        return {Error(API_INVALID_ARGUMENT, "Start height must be less than end height."), 400};
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

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("transactions");
        writer.StartArray();
        {
            for (const auto &tx : result)
            {
                publicKeysToAddresses(tx, writer);
            }
        }
        writer.EndArray();
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getTransactionDetails(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    std::string hashStr = req.matches[1];

    Crypto::Hash hash;

    Common::podFromHex(hashStr, hash.data);

    for (const auto &tx : m_walletBackend->getTransactions())
    {
        if (tx.hash == hash)
        {
            rapidjson::StringBuffer sb;

            rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

            writer.StartObject();
            {
                writer.Key("transaction");
                publicKeysToAddresses(tx, writer);
            }
            writer.EndObject();

            res.body = sb.GetString();

            return {SUCCESS, 200};
        }
    }

    /* Not found */
    return {SUCCESS, 404};
}

std::tuple<Error, uint16_t> ApiDispatcher::getTransactionsByPaymentId(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    std::string paymentID = req.matches[1];

    std::vector<WalletTypes::Transaction> transactions;

    for (const auto &tx : m_walletBackend->getTransactions())
    {
        if (tx.paymentID == paymentID)
        {
            transactions.push_back(tx);
        }
    }

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("transactions");
        writer.StartArray();
        {
            for (const auto &tx : transactions)
            {
                publicKeysToAddresses(tx, writer);
            }
        }
        writer.EndArray();
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getTransactionsWithPaymentId(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    std::vector<WalletTypes::Transaction> transactions;

    for (const auto &tx : m_walletBackend->getTransactions())
    {
        if (tx.paymentID != "")
        {
            transactions.push_back(tx);
        }
    }

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("transactions");
        writer.StartArray();
        {
            for (const auto &tx : transactions)
            {
                publicKeysToAddresses(tx, writer);
            }
        }
        writer.EndArray();
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getBalance(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    const auto [unlocked, locked] = m_walletBackend->getTotalBalance();

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("unlocked");
        writer.Uint64(unlocked);

        writer.Key("locked");
        writer.Uint64(locked);
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getBalanceForAddress(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    std::string address = req.matches[1];

    const auto [error, unlocked, locked] = m_walletBackend->getBalance(address);

    if (error)
    {
        return {error, 400};
    }

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("unlocked");
        writer.Uint64(unlocked);

        writer.Key("locked");
        writer.Uint64(locked);
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getBalances(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    const auto balances = m_walletBackend->getBalances();

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartArray();
    {
        for (const auto &[address, unlocked, locked] : balances)
        {
            writer.StartObject();
            {
                writer.Key("address");
                writer.String(address);

                writer.Key("unlocked");
                writer.Uint64(unlocked);

                writer.Key("locked");
                writer.Uint64(locked);
            }
            writer.EndObject();
        }
    }
    writer.EndArray();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> ApiDispatcher::getTxPrivateKey(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body) const
{
    std::string txHashStr = req.matches[1];

    Crypto::Hash txHash;

    Common::podFromHex(txHashStr, txHash.data);

    const auto [error, key] = m_walletBackend->getTxPrivateKey(txHash);

    if (error)
    {
        return {error, 400};
    }

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("transactionPrivateKey");
        key.toJSON(writer);
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

//////////////////////
/* OPTIONS REQUESTS */
//////////////////////

void ApiDispatcher::handleOptions(const httplib::Request &req, httplib::Response &res) const
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

std::tuple<std::string, uint16_t, bool, std::string, std::string>
    ApiDispatcher::getDefaultWalletParams(const rapidjson::Document &body) const
{
    std::string daemonHost = "127.0.0.1";

    uint16_t daemonPort = CryptoNote::RPC_DEFAULT_PORT;

    bool daemonSSL = false;

    const std::string filename = getStringFromJSON(body, "filename");

    const std::string password = getStringFromJSON(body, "password");

    if (hasMember(body, "daemonHost"))
    {
        daemonHost = getStringFromJSON(body, "daemonHost");
    }

    if (hasMember(body, "daemonPort"))
    {
        daemonPort = getUintFromJSON(body, "daemonPort");
    }

    if (hasMember(body, "daemonSSL"))
    {
        daemonSSL = getBoolFromJSON(body, "daemonSSL");
    }

    return {daemonHost, daemonPort, daemonSSL, filename, password};
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

void ApiDispatcher::publicKeysToAddresses(
    const WalletTypes::Transaction &transaction,
    rapidjson::Writer<rapidjson::StringBuffer> &writer) const
{
    // Duplicated from <include/WalletTypes.h> to supply the address instead of publicKey in the transfer
    writer.StartObject();
    {
        writer.Key("transfers");
        writer.StartArray();
        {
            for (const auto &[publicKey, amount] : transaction.transfers)
            {
                const auto [error, address] = m_walletBackend->getAddress(publicKey);

                writer.StartObject();
                {
                    writer.Key("address");
                    writer.String(address);

                    writer.Key("amount");
                    writer.Int64(amount);
                }
                writer.EndObject();
            }
        }
        writer.EndArray();

        writer.Key("hash");
        transaction.hash.toJSON(writer);

        writer.Key("fee");
        writer.Uint64(transaction.fee);

        writer.Key("blockHeight");
        writer.Uint64(transaction.blockHeight);

        writer.Key("timestamp");
        writer.Uint64(transaction.timestamp);

        writer.Key("paymentID");
        writer.String(transaction.paymentID);

        writer.Key("unlockTime");
        writer.Uint64(transaction.unlockTime);

        writer.Key("isCoinbaseTransaction");
        writer.Bool(transaction.isCoinbaseTransaction);
    }
    writer.EndObject();
}

std::string ApiDispatcher::hashPassword(const std::string password) const
{
    using namespace CryptoPP;

    /* Using SHA256 as the algorithm */
    CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> pbkdf2;

    byte key[16];

    /* Hash the password with pbkdf2 */
    pbkdf2.DeriveKey(
        key,
        sizeof(key),
        0,
        (byte *)password.c_str(),
        password.size(),
        m_salt,
        sizeof(m_salt),
        ApiConstants::PBKDF2_ITERATIONS);

    return Common::podToHex(key);
}
