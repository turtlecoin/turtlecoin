// Copyright (c) 2019-2020, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

//////////////////////////
#include <rpc/RpcServer.h>
//////////////////////////

#include "version.h"

#include <common/CryptoNoteTools.h>
#include <config/Constants.h>
#include <errors/ValidateParameters.h>
#include <iostream>
#include <logger/Logger.h>
#include <serialization/SerializationTools.h>
#include <utilities/Addresses.h>
#include <utilities/ColouredMsg.h>
#include <utilities/FormatTools.h>
#include <utilities/ParseExtra.h>

RpcServer::RpcServer(
    const uint16_t bindPort,
    const std::string rpcBindIp,
    const std::string corsHeader,
    const std::string feeAddress,
    const uint64_t feeAmount,
    const RpcMode rpcMode,
    const std::shared_ptr<CryptoNote::Core> core,
    const std::shared_ptr<CryptoNote::NodeServer> p2p,
    const std::shared_ptr<CryptoNote::ICryptoNoteProtocolHandler> syncManager):
    m_port(bindPort),
    m_host(rpcBindIp),
    m_corsHeader(corsHeader),
    m_feeAddress(feeAddress),
    m_feeAmount(feeAmount),
    m_rpcMode(rpcMode),
    m_core(core),
    m_p2p(p2p),
    m_syncManager(syncManager)
{
    if (m_feeAddress != "")
    {
        Error error = validateAddresses({m_feeAddress}, false);

        if (error != SUCCESS)
        {
            std::cout << WarningMsg("Fee address given is not valid: " + error.getErrorMessage()) << std::endl;

            exit(1);
        }
    }

    const bool bodyRequired = true;

    const bool bodyNotRequired = false;

    const bool syncRequired = true;

    const bool syncNotRequired = false;

    /* Route the request through our middleware function, before forwarding
       to the specified function */
    const auto router =
        [this](
            const auto function, const RpcMode routePermissions, const bool isBodyRequired, const bool syncRequired) {
            return [=](const httplib::Request &req, httplib::Response &res) {
                /* Pass the inputted function with the arguments passed through
                   to middleware */
                middleware(
                    req,
                    res,
                    routePermissions,
                    isBodyRequired,
                    syncRequired,
                    std::bind(function, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            };
        };

    m_server
        .Post("/block", router(&RpcServer::submitBlock, RpcMode::Default, bodyRequired, syncRequired))

        .Get(
            "/block/" + m_hashRegex, /* /block/{hash} */
            router(&RpcServer::getBlockHeaderByHash, RpcMode::BlockExplorerEnabled, bodyNotRequired, syncNotRequired))

        .Get(
            "/block/\\d+", /* /block/{height} */
            router(&RpcServer::getBlockHeaderByHeight, RpcMode::BlockExplorerEnabled, bodyNotRequired, syncNotRequired))

        .Get(
            "/block/" + m_hashRegex + "/raw", /* /block/{hash}/raw */
            router(&RpcServer::getRawBlockByHash, RpcMode::BlockExplorerEnabled, bodyNotRequired, syncNotRequired))

        .Get(
            "/block/\\d+/raw", /* /block/{height}/raw */
            router(&RpcServer::getRawBlockByHeight, RpcMode::BlockExplorerEnabled, bodyNotRequired, syncNotRequired))

        .Get("/block/count", router(&RpcServer::getBlockCount, RpcMode::Default, bodyNotRequired, syncNotRequired))

        .Get(
            "/block/headers/\\d+", /* /block/headers/{height} */
            router(&RpcServer::getBlocksByHeight, RpcMode::BlockExplorerEnabled, bodyNotRequired, syncNotRequired))

        .Get(
            "/block/last", /* /block/header/{hash} */
            router(&RpcServer::getLastBlockHeader, RpcMode::Default, bodyNotRequired, syncNotRequired))

        .Post("/block/template", router(&RpcServer::getBlockTemplate, RpcMode::Default, bodyRequired, syncRequired))

        .Get("/fee", router(&RpcServer::fee, RpcMode::Default, bodyNotRequired, syncNotRequired))

        .Get("/height", router(&RpcServer::height, RpcMode::Default, bodyNotRequired, syncNotRequired))

        .Get(
            "/indexes/\\d+/\\d+",
            router(&RpcServer::getGlobalIndexes, RpcMode::Default, bodyNotRequired, syncNotRequired))

        .Post("/indexes/random", router(&RpcServer::getRandomOuts, RpcMode::Default, bodyRequired, syncNotRequired))

        .Get("/info", router(&RpcServer::info, RpcMode::Default, bodyNotRequired, syncNotRequired))

        .Get("/peers", router(&RpcServer::peers, RpcMode::Default, bodyNotRequired, syncNotRequired))

        .Post("/sync", router(&RpcServer::getWalletSyncData, RpcMode::Default, bodyRequired, syncNotRequired))

        .Post("/sync/raw", router(&RpcServer::getRawBlocks, RpcMode::Default, bodyRequired, syncNotRequired))

        .Post(
            "/transaction",
            router(&RpcServer::sendTransaction, RpcMode::BlockExplorerEnabled, bodyRequired, syncRequired))

        .Get(
            "/transaction/" + m_hashRegex, /* /transaction/{hash} */
            router(
                &RpcServer::getTransactionDetailsByHash,
                RpcMode::BlockExplorerEnabled,
                bodyNotRequired,
                syncNotRequired))

        .Get(
            "/transaction/" + m_hashRegex + "/raw", /* /transaction/{hash}/raw */
            router(
                &RpcServer::getRawTransactionByHash, RpcMode::BlockExplorerEnabled, bodyNotRequired, syncNotRequired))

        .Get(
            "/transaction/pool",
            router(&RpcServer::getTransactionsInPool, RpcMode::BlockExplorerEnabled, bodyNotRequired, syncNotRequired))

        .Post(
            "/transaction/pool/delta",
            router(&RpcServer::getPoolChanges, RpcMode::Default, bodyRequired, syncNotRequired))

        .Get(
            "/transaction/pool/raw",
            router(
                &RpcServer::getRawTransactionsInPool, RpcMode::BlockExplorerEnabled, bodyNotRequired, syncNotRequired))

        .Post(
            "/transaction/status",
            router(&RpcServer::getTransactionsStatus, RpcMode::Default, bodyRequired, syncNotRequired))

        /* Matches everything */
        /* NOTE: Not passing through middleware */
        .Options(".*", [this](auto &req, auto &res) { handleOptions(req, res); });
}

RpcServer::~RpcServer()
{
    stop();
}

void RpcServer::start()
{
    m_serverThread = std::thread(&RpcServer::listen, this);
}

void RpcServer::listen()
{
    const auto listenError = m_server.listen(m_host, m_port);

    if (listenError != httplib::SUCCESS)
    {
        std::cout << WarningMsg("Failed to start RPC server: ")
                  << WarningMsg(httplib::detail::getSocketErrorMessage(listenError)) << std::endl;

        exit(1);
    }
}

void RpcServer::stop()
{
    m_server.stop();

    if (m_serverThread.joinable())
    {
        m_serverThread.join();
    }
}

std::tuple<std::string, uint16_t> RpcServer::getConnectionInfo()
{
    return {m_host, m_port};
}

std::optional<rapidjson::Document>
    RpcServer::getJsonBody(const httplib::Request &req, httplib::Response &res, const bool bodyRequired)
{
    rapidjson::Document jsonBody;

    if (!bodyRequired)
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

void RpcServer::middleware(
    const httplib::Request &req,
    httplib::Response &res,
    const RpcMode routePermissions,
    const bool bodyRequired,
    const bool syncRequired,
    std::function<std::tuple<Error, uint16_t>(
        const httplib::Request &req,
        httplib::Response &res,
        const rapidjson::Document &body)> handler)
{
    Logger::logger.log(
        "[" + req.get_header_value("REMOTE_ADDR") + "] Incoming " + req.method + " request: " + req.path
            + ", User-Agent: " + req.get_header_value("User-Agent"),
        Logger::DEBUG,
        {Logger::DAEMON_RPC});

    if (m_corsHeader != "")
    {
        res.set_header("Access-Control-Allow-Origin", m_corsHeader);
    }

    res.set_header("Content-Type", "application/json");

    const auto jsonBody = getJsonBody(req, res, bodyRequired);

    if (!jsonBody)
    {
        return;
    }

    /* If this route requires higher permissions than we have enabled, then
     * reject the request */
    if (routePermissions > m_rpcMode)
    {
        failRequest(Error(API_BLOCKEXPLORER_DISABLED), res);

        res.status = 403;

        return;
    }

    const uint64_t height = m_core->getTopBlockIndex() + 1;

    const uint64_t networkHeight = std::max(1u, m_syncManager->getBlockchainHeight());

    const bool areSynced = m_p2p->get_payload_object().isSynchronized() && height >= networkHeight;

    if (syncRequired && !areSynced)
    {
        failRequest(Error(API_NODE_NOT_SYNCED), res);

        res.status = 503;

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
    catch (const std::invalid_argument &e)
    {
        Logger::logger.log(
            "Caught JSON exception, likely missing required json parameter: " + std::string(e.what()),
            Logger::FATAL,
            {Logger::DAEMON_RPC});

        failRequest(Error(API_INVALID_ARGUMENT, e.what()), res);

        res.status = 400;
    }
    catch (const std::exception &e)
    {
        std::stringstream error;

        error << "Caught unexpected exception: " << e.what() << " while processing " << req.path
              << " request for User-Agent: " << req.get_header_value("User-Agent");

        Logger::logger.log(error.str(), Logger::FATAL, {Logger::DAEMON_RPC});

        if (req.body != "")
        {
            Logger::logger.log("Body: " + req.body, Logger::FATAL, {Logger::DAEMON_RPC});
        }

        failRequest(Error(API_INTERNAL_ERROR, e.what()), res);

        res.status = 500;
    }
}

void RpcServer::failRequest(
    const Error error,
    httplib::Response &res)
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

void RpcServer::handleOptions(const httplib::Request &req, httplib::Response &res) const
{
    Logger::logger.log("Incoming " + req.method + " request: " + req.path, Logger::DEBUG, {Logger::DAEMON_RPC});

    std::string supported = "OPTIONS, GET, POST";

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

    if (m_corsHeader != "")
    {
        res.set_header("Access-Control-Allow-Origin", m_corsHeader);

        res.set_header("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    }

    res.status = 200;
}

std::tuple<Error, uint16_t>
    RpcServer::info(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    const uint64_t height = m_core->getTopBlockIndex() + 1;

    const uint64_t networkHeight = std::max(1u, m_syncManager->getBlockchainHeight());

    const auto blockDetails = m_core->getBlockDetails(height - 1);

    const uint64_t difficulty = m_core->getDifficultyForNextBlock();

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        uint64_t total_conn = m_p2p->get_connections_count();
        uint64_t outgoing_connections_count = m_p2p->get_outgoing_connections_count();

        writer.Key("alternateBlockCount");
        writer.Uint64(m_core->getAlternativeBlockCount());

        writer.Key("difficulty");
        writer.Uint64(difficulty);

        writer.Key("greyPeerlistSize");
        writer.Uint64(m_p2p->getPeerlistManager().get_gray_peers_count());

        writer.Key("hashrate");
        writer.Uint64(round(difficulty / CryptoNote::parameters::DIFFICULTY_TARGET));

        writer.Key("height");
        writer.Uint64(height);

        writer.Key("incomingConnections");
        writer.Uint64(total_conn - outgoing_connections_count);

        writer.Key("lastBlockIndex");
        writer.Uint64(std::max(1u, m_syncManager->getObservedHeight()) - 1);

        writer.Key("majorVersion");
        writer.Uint64(blockDetails.majorVersion);

        writer.Key("minorVersion");
        writer.Uint64(blockDetails.minorVersion);

        writer.Key("networkHeight");
        writer.Uint64(networkHeight);

        writer.Key("outgoingConnections");
        writer.Uint64(outgoing_connections_count);

        writer.Key("startTime");
        writer.Uint64(m_core->getStartTime());

        writer.Key("supportedHeight");
        writer.Uint64(
            CryptoNote::parameters::FORK_HEIGHTS_SIZE == 0
                ? 0
                : CryptoNote::parameters::FORK_HEIGHTS[CryptoNote::parameters::CURRENT_FORK_INDEX]);

        writer.Key("synced");
        writer.Bool(height == networkHeight);

        writer.Key("transactionsPoolSize");
        writer.Uint64(m_core->getPoolTransactionCount());

        writer.Key("transactionsSize");
        /* Transaction count without coinbase transactions - one per block, so subtract height */
        writer.Uint64(m_core->getBlockchainTransactionCount() - height);

        writer.Key("upgradeHeights");
        writer.StartArray();
        {
            for (const uint64_t height : CryptoNote::parameters::FORK_HEIGHTS)
            {
                writer.Uint64(height);
            }
        }
        writer.EndArray();

        writer.Key("version");
        writer.String(PROJECT_VERSION);

        writer.Key("whitePeerlistSize");
        writer.Uint64(m_p2p->getPeerlistManager().get_white_peers_count());
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t>
    RpcServer::fee(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("address");
        writer.String(m_feeAddress);

        writer.Key("amount");
        writer.Uint64(m_feeAmount);
    }

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t>
    RpcServer::height(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("height");
        writer.Uint64(m_core->getTopBlockIndex() + 1);

        writer.Key("networkHeight");
        writer.Uint64(std::max(1u, m_syncManager->getBlockchainHeight()));
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t>
    RpcServer::peers(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    std::list<PeerlistEntry> peers_white;

    std::list<PeerlistEntry> peers_gray;

    m_p2p->getPeerlistManager().get_peerlist_full(peers_gray, peers_white);

    writer.StartObject();
    {
        writer.Key("greyPeers");
        writer.StartArray();
        {
            for (const auto &peer : peers_gray)
            {
                std::stringstream stream;

                stream << peer.adr;

                writer.String(stream.str());
            }
        }
        writer.EndArray();

        writer.Key("peers");
        writer.StartArray();
        {
            for (const auto &peer : peers_white)
            {
                std::stringstream stream;

                stream << peer.adr;

                writer.String(stream.str());
            }
        }
        writer.EndArray();
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t>
    RpcServer::sendTransaction(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    std::vector<uint8_t> transaction;

    const std::string rawData = getStringFromJSON(body);

    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    if (!Common::fromHex(rawData, transaction))
    {
        return {Error(API_INVALID_ARGUMENT, "Failed to parse transaction from hex buffer"), 400};
    }

    const CryptoNote::CachedTransaction cachedTransaction(transaction);

    const auto hash = cachedTransaction.getTransactionHash();

    std::stringstream stream;

    stream << "Attempting to add transaction " << hash << " from /transaction to pool";

    Logger::logger.log(stream.str(), Logger::DEBUG, {Logger::DAEMON_RPC});

    const auto [success, error] = m_core->addTransactionToPool(transaction);

    if (!success)
    {
        return {Error(API_TRANSACTION_POOL_INSERT_FAILED, error), 409};
    }

    m_syncManager->relayTransactions({transaction});

    hash.toJSON(writer);

    res.body = sb.GetString();

    return {SUCCESS, 202};
}

std::tuple<Error, uint16_t>
    RpcServer::getRandomOuts(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    const uint64_t numOutputs = getUint64FromJSON(body, "count");

    writer.StartArray();
    {
        for (const auto &jsonAmount : getArrayFromJSON(body, "amounts"))
        {
            writer.StartObject();

            const uint64_t amount = jsonAmount.GetUint64();

            std::vector<uint32_t> globalIndexes;

            std::vector<Crypto::PublicKey> publicKeys;

            const auto [success, error] =
                m_core->getRandomOutputs(amount, static_cast<uint16_t>(numOutputs), globalIndexes, publicKeys);

            if (!success)
            {
                return {Error(CANT_GET_FAKE_OUTPUTS, error), 500};
            }

            if (globalIndexes.size() != numOutputs)
            {
                std::stringstream stream;

                stream
                    << "Failed to get enough matching outputs for amount " << amount << " ("
                    << Utilities::formatAmount(amount) << "). Requested outputs: " << numOutputs
                    << ", found outputs: " << globalIndexes.size()
                    << ". Further explanation here: https://gist.github.com/zpalmtree/80b3e80463225bcfb8f8432043cb594c"
                    << std::endl
                    << "Note: If you are a public node operator, you can safely ignore this message. "
                    << "It is only relevant to the user sending the transaction.";

                return {Error(CANT_GET_FAKE_OUTPUTS, stream.str()), 416};
            }

            writer.Key("amount");
            writer.Uint64(amount);

            writer.Key("outputs");
            writer.StartArray();
            {
                for (size_t i = 0; i < globalIndexes.size(); i++)
                {
                    writer.StartObject();
                    {
                        writer.Key("index");
                        writer.Uint64(globalIndexes[i]);

                        writer.Key("key");
                        publicKeys[i].toJSON(writer);
                    }
                    writer.EndObject();
                }
            }
            writer.EndArray();

            writer.EndObject();
        }
    }
    writer.EndArray();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t>
    RpcServer::getWalletSyncData(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    std::vector<Crypto::Hash> blockHashCheckpoints;

    if (hasMember(body, "checkpoints"))
    {
        for (const auto &jsonHash : getArrayFromJSON(body, "checkpoints"))
        {
            std::string hashStr = jsonHash.GetString();

            Crypto::Hash hash;

            Common::podFromHex(hashStr, hash);

            blockHashCheckpoints.push_back(hash);
        }
    }

    const uint64_t startHeight = hasMember(body, "height") ? getUint64FromJSON(body, "height") : 0;

    const uint64_t startTimestamp = hasMember(body, "timestamp") ? getUint64FromJSON(body, "timestamp") : 0;

    const uint64_t blockCount = hasMember(body, "count") ? getUint64FromJSON(body, "count") : 100;

    const bool skipCoinbaseTransactions =
        hasMember(body, "skipCoinbaseTransactions") ? getBoolFromJSON(body, "skipCoinbaseTransactions") : false;

    std::vector<WalletTypes::WalletBlockInfo> walletBlocks;

    std::optional<WalletTypes::TopBlock> topBlockInfo;

    const bool success = m_core->getWalletSyncData(
        blockHashCheckpoints,
        startHeight,
        startTimestamp,
        blockCount,
        skipCoinbaseTransactions,
        walletBlocks,
        topBlockInfo);

    if (!success)
    {
        return {Error(API_INTERNAL_ERROR), 500};
    }

    writer.StartObject();
    {
        writer.Key("blocks");
        writer.StartArray();
        {
            for (const auto &block : walletBlocks)
            {
                writer.StartObject();

                writer.Key("hash");
                block.blockHash.toJSON(writer);

                writer.Key("height");
                writer.Uint64(block.blockHeight);

                writer.Key("timestamp");
                writer.Uint64(block.blockTimestamp);

                if (block.coinbaseTransaction)
                {
                    writer.Key("coinbaseTX");
                    writer.StartObject();
                    {
                        writer.Key("hash");
                        block.coinbaseTransaction->hash.toJSON(writer);

                        writer.Key("outputs");
                        writer.StartArray();
                        {
                            for (const auto &output : block.coinbaseTransaction->keyOutputs)
                            {
                                writer.StartObject();
                                {
                                    writer.Key("amount");
                                    writer.Uint64(output.amount);

                                    writer.Key("key");
                                    output.key.toJSON(writer);
                                }
                                writer.EndObject();
                            }
                        }
                        writer.EndArray();

                        writer.Key("publicKey");
                        block.coinbaseTransaction->transactionPublicKey.toJSON(writer);

                        writer.Key("unlockTime");
                        writer.Uint64(block.coinbaseTransaction->unlockTime);
                    }
                    writer.EndObject();
                }

                writer.Key("transactions");
                writer.StartArray();
                {
                    for (const auto &transaction : block.transactions)
                    {
                        writer.StartObject();
                        {
                            writer.Key("hash");
                            transaction.hash.toJSON(writer);

                            writer.Key("inputs");
                            writer.StartArray();
                            {
                                for (const auto &input : transaction.keyInputs)
                                {
                                    writer.StartObject();
                                    {
                                        writer.Key("amount");
                                        writer.Uint64(input.amount);

                                        writer.Key("keyImage");
                                        input.keyImage.toJSON(writer);
                                    }
                                    writer.EndObject();
                                }
                            }
                            writer.EndArray();

                            writer.Key("outputs");
                            writer.StartArray();
                            {
                                for (const auto &output : transaction.keyOutputs)
                                {
                                    writer.StartObject();
                                    {
                                        writer.Key("amount");
                                        writer.Uint64(output.amount);

                                        writer.Key("key");
                                        output.key.toJSON(writer);
                                    }
                                    writer.EndObject();
                                }
                            }
                            writer.EndArray();

                            writer.Key("paymentID");
                            writer.String(transaction.paymentID);

                            writer.Key("publicKey");
                            transaction.transactionPublicKey.toJSON(writer);

                            writer.Key("unlockTime");
                            writer.Uint64(transaction.unlockTime);
                        }
                        writer.EndObject();
                    }
                }
                writer.EndArray();

                writer.EndObject();
            }
        }
        writer.EndArray();

        writer.Key("synced");
        writer.Bool(walletBlocks.empty());

        if (topBlockInfo)
        {
            writer.Key("topBlock");
            writer.StartObject();
            {
                writer.Key("hash");
                topBlockInfo->hash.toJSON(writer);

                writer.Key("height");
                writer.Uint64(topBlockInfo->height);
            }
            writer.EndObject();
        }
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t>
    RpcServer::getGlobalIndexes(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    std::string stripped = req.path.substr(std::string("/indexes/").size());

    uint64_t splitPos = stripped.find_first_of("/");

    /* Take all the chars before the "/", this is our start height */
    std::string startHeightStr = stripped.substr(0, splitPos);

    /* Take all the chars after the "/", this is our end height */
    std::string endHeightStr = stripped.substr(splitPos + 1);

    uint64_t startHeight;

    uint64_t endHeight;

    try
    {
        startHeight = std::stoull(startHeightStr);

        endHeight = std::stoull(endHeightStr);

        if (startHeight >= endHeight)
        {
            return {Error(API_INVALID_ARGUMENT, "Start height cannot be greater than end height."), 400};
        }
    }
    catch (const std::out_of_range &)
    {
        return {Error(API_INVALID_ARGUMENT), 400};
    }
    catch (const std::invalid_argument &)
    {
        return {Error(API_INVALID_ARGUMENT), 400};
    }

    std::unordered_map<Crypto::Hash, std::vector<uint64_t>> indexes;

    const bool success = m_core->getGlobalIndexesForRange(startHeight, endHeight, indexes);

    if (!success)
    {
        return {Error(API_INTERNAL_ERROR, "Cannot retrieve global indexes for range."), 500};
    }

    writer.StartArray();
    {
        for (const auto [hash, globalIndexes] : indexes)
        {
            writer.StartObject();
            {
                writer.Key("hash");
                hash.toJSON(writer);

                writer.Key("indexes");
                writer.StartArray();
                {
                    for (const auto index : globalIndexes)
                    {
                        writer.Uint64(index);
                    }
                }
                writer.EndArray();
            }
            writer.EndObject();
        }
    }
    writer.EndArray();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t>
    RpcServer::getBlockTemplate(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    const uint64_t reserveSize = getUint64FromJSON(body, "reserveSize");

    if (reserveSize > 255)
    {
        return {Error(API_INVALID_ARGUMENT, "Reserved size is too large, maximum permitted is 255."), 400};
    }

    const std::string address = getStringFromJSON(body, "address");

    Error addressError = validateAddresses({address}, false);

    if (addressError)
    {
        return {Error(API_INVALID_ARGUMENT, addressError.getErrorMessage()), 400};
    }

    const auto [publicSpendKey, publicViewKey] = Utilities::addressToKeys(address);

    CryptoNote::BlockTemplate blockTemplate;

    std::vector<uint8_t> blobReserve;

    blobReserve.resize(reserveSize, 0);

    uint64_t difficulty;

    uint32_t height;

    const auto [success, error] =
        m_core->getBlockTemplate(blockTemplate, publicViewKey, publicSpendKey, blobReserve, difficulty, height);

    if (!success)
    {
        return {Error(API_INTERNAL_ERROR, "Failed to create block template: " + error), 500};
    }

    std::vector<uint8_t> blockBlob = CryptoNote::toBinaryArray(blockTemplate);

    const auto transactionPublicKey = Utilities::getTransactionPublicKeyFromExtra(blockTemplate.baseTransaction.extra);

    uint64_t reservedOffset = 0;

    if (reserveSize > 0)
    {
        /* Find where in the block blob the transaction public key is */
        const auto it = std::search(
            blockBlob.begin(),
            blockBlob.end(),
            std::begin(transactionPublicKey.data),
            std::end(transactionPublicKey.data));

        /* The reserved offset is past the transactionPublicKey, then past
         * the extra nonce tags */
        reservedOffset = (it - blockBlob.begin()) + sizeof(transactionPublicKey) + 2;

        if (reservedOffset + reserveSize > blockBlob.size())
        {
            return {Error(API_INTERNAL_ERROR,
                          "Internal error: failed to create block template, not enough space for reserved bytes"), 500};
        }
    }

    writer.StartObject();
    {
        writer.Key("difficulty");
        writer.Uint64(difficulty);

        writer.Key("height");
        writer.Uint(height);

        writer.Key("reservedOffset");
        writer.Uint64(reservedOffset);

        writer.Key("blob");
        writer.String(Common::toHex(blockBlob));
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 201};
}

std::tuple<Error, uint16_t>
    RpcServer::submitBlock(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    const auto blockBlob = getStringFromJSON(body);

    std::vector<uint8_t> rawBlob;

    if (!Common::fromHex(blockBlob, rawBlob))
    {
        return {Error(API_INVALID_ARGUMENT, "Submitted block blob is not hex!"), 400};
    }

    const auto submitResult = m_core->submitBlock(rawBlob);

    if (submitResult != CryptoNote::error::AddBlockErrorCondition::BLOCK_ADDED)
    {
        return {Error(API_BLOCK_NOT_ACCEPTED), 409};
    }

    if (submitResult == CryptoNote::error::AddBlockErrorCode::ADDED_TO_MAIN
        || submitResult == CryptoNote::error::AddBlockErrorCode::ADDED_TO_ALTERNATIVE_AND_SWITCHED)
    {
        CryptoNote::NOTIFY_NEW_BLOCK::request newBlockMessage;

        CryptoNote::BlockTemplate blockTemplate;

        CryptoNote::fromBinaryArray(blockTemplate, rawBlob);

        newBlockMessage.block = CryptoNote::RawBlockLegacy(rawBlob, blockTemplate, m_core);

        newBlockMessage.hop = 0;

        newBlockMessage.current_blockchain_height = m_core->getTopBlockIndex() + 1;

        m_syncManager->relayBlock(newBlockMessage);

        const CryptoNote::CachedBlock block(blockTemplate);

        const auto hash = block.getBlockHash();

        hash.toJSON(writer);

        res.body = sb.GetString();
    }

    return {SUCCESS, 202};
}

std::tuple<Error, uint16_t>
    RpcServer::getBlockCount(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.Uint64(m_core->getTopBlockIndex() + 1);

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

uint64_t RpcServer::calculateTotalFeeAmount(const std::vector<Crypto::Hash> &transactionHashes)
{
    uint64_t totalFeeAmount = 0;

    std::vector<Crypto::Hash> ignore;

    std::vector<std::vector<uint8_t>> transactions;

    m_core->getTransactions(transactionHashes, transactions, ignore);

    for (const std::vector<uint8_t> rawTX : transactions)
    {
        {
            CryptoNote::Transaction tx;

            fromBinaryArray(tx, rawTX);

            const uint64_t outputAmount =
                std::accumulate(tx.outputs.begin(), tx.outputs.end(), 0ull, [](const auto acc, const auto out) {
                    return acc + out.amount;
                });

            const uint64_t inputAmount =
                std::accumulate(tx.inputs.begin(), tx.inputs.end(), 0ull, [](const auto acc, const auto in) {
                    if (in.type() == typeid(CryptoNote::KeyInput))
                    {
                        return acc + boost::get<CryptoNote::KeyInput>(in).amount;
                    }

                    return acc;
                });

            const uint64_t fee = inputAmount - outputAmount;

            totalFeeAmount += fee;
        }
    }

    return totalFeeAmount;
}

void RpcServer::generateBlockHeader(
    const Crypto::Hash &blockHash,
    rapidjson::Writer<rapidjson::StringBuffer> &writer,
    const bool headerOnly)
{
    const auto topHeight = m_core->getTopBlockIndex();

    CryptoNote::BlockTemplate block = m_core->getBlockByHash(blockHash);

    CryptoNote::CachedBlock cachedBlock(block);

    const auto height = cachedBlock.getBlockIndex();

    const auto outputs = block.baseTransaction.outputs;

    const auto extraDetails = m_core->getBlockDetails(blockHash);

    const uint64_t reward = std::accumulate(
        outputs.begin(), outputs.end(), 0ull, [](const auto acc, const auto out) { return acc + out.amount; });

    const uint64_t totalFeeAmount = calculateTotalFeeAmount(block.transactionHashes);

    std::vector<Crypto::Hash> ignore;

    std::vector<std::vector<uint8_t>> transactions;

    m_core->getTransactions(block.transactionHashes, transactions, ignore);

    writer.StartObject();
    {
        writer.Key("alreadyGeneratedCoins");
        writer.String(std::to_string(extraDetails.alreadyGeneratedCoins));

        writer.Key("alreadyGeneratedTransactions");
        writer.Uint64(extraDetails.alreadyGeneratedTransactions);

        writer.Key("baseReward");
        writer.Uint64(extraDetails.baseReward);

        writer.Key("depth");
        writer.Uint64(topHeight - height);

        writer.Key("difficulty");
        writer.Uint64(m_core->getBlockDifficulty(height));

        writer.Key("hash");
        blockHash.toJSON(writer);

        writer.Key("height");
        writer.Uint64(height);

        writer.Key("majorVersion");
        writer.Uint64(block.majorVersion);

        writer.Key("minorVersion");
        writer.Uint64(block.minorVersion);

        writer.Key("nonce");
        writer.Uint64(block.nonce);

        writer.Key("orphan");
        writer.Bool(extraDetails.isAlternative);

        writer.Key("prevHash");
        block.previousBlockHash.toJSON(writer);

        writer.Key("reward");
        writer.Uint64(reward);

        writer.Key("size");
        writer.Uint64(extraDetails.blockSize);

        writer.Key("sizeMedian");
        writer.Uint64(extraDetails.sizeMedian);

        writer.Key("timestamp");
        writer.Uint64(block.timestamp);

        writer.Key("totalFeeAmount");
        writer.Uint64(totalFeeAmount);

        writer.Key("transactionCount");
        writer.Uint64(extraDetails.transactions.size());

        /* If we are not part of a sub-object (such as /transaction) then we can
         * include basic information about the transactions */
        if (!headerOnly)
        {
            writer.Key("transactions");
            writer.StartArray();
            {
                /* Coinbase transaction */
                writer.StartObject();
                {
                    const auto txOutputs = block.baseTransaction.outputs;

                    const uint64_t outputAmount =
                        std::accumulate(txOutputs.begin(), txOutputs.end(), 0ull, [](const auto acc, const auto out) {
                            return acc + out.amount;
                        });

                    writer.Key("amountOut");
                    writer.Uint64(outputAmount);

                    writer.Key("fee");
                    writer.Uint64(0);

                    writer.Key("hash");
                    const auto baseTransactionBranch = getObjectHash(block.baseTransaction);
                    baseTransactionBranch.toJSON(writer);

                    writer.Key("size");
                    writer.Uint64(getObjectBinarySize(block.baseTransaction));
                }
                writer.EndObject();

                for (const std::vector<uint8_t> rawTX : transactions)
                {
                    writer.StartObject();
                    {
                        CryptoNote::Transaction tx;

                        fromBinaryArray(tx, rawTX);

                        const uint64_t outputAmount = std::accumulate(
                            tx.outputs.begin(), tx.outputs.end(), 0ull, [](const auto acc, const auto out) {
                                return acc + out.amount;
                            });

                        const uint64_t inputAmount = std::accumulate(
                            tx.inputs.begin(), tx.inputs.end(), 0ull, [](const auto acc, const auto in) {
                                if (in.type() == typeid(CryptoNote::KeyInput))
                                {
                                    return acc + boost::get<CryptoNote::KeyInput>(in).amount;
                                }

                                return acc;
                            });

                        const uint64_t fee = inputAmount - outputAmount;

                        writer.Key("amountOut");
                        writer.Uint64(outputAmount);

                        writer.Key("fee");
                        writer.Uint64(fee);

                        writer.Key("hash");
                        const auto txHash = getObjectHash(tx);
                        txHash.toJSON(writer);

                        writer.Key("size");
                        writer.Uint64(getObjectBinarySize(tx));
                    }
                    writer.EndObject();
                }
            }
            writer.EndArray();
        }

        writer.Key("transactionsCumulativeSize");
        writer.Uint64(extraDetails.transactionsCumulativeSize);
    }
    writer.EndObject();
}

std::tuple<Error, uint16_t>
    RpcServer::getLastBlockHeader(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    try
    {
        const auto height = m_core->getTopBlockIndex();

        const auto hash = m_core->getBlockHashByIndex(height);

        generateBlockHeader(hash, writer);

        res.body = sb.GetString();

        return {SUCCESS, 200};
    }
    catch (const std::exception &)
    {
        return {Error(API_INTERNAL_ERROR, "Could not retrieve last block header."), 500};
    }
}

std::tuple<Error, uint16_t> RpcServer::getBlockHeaderByHash(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    std::string hashStr = req.path.substr(std::string("/block/").size());

    Crypto::Hash hash;

    if (!Common::podFromHex(hashStr, hash))
    {
        return {Error(API_INVALID_ARGUMENT), 400};
    }

    const auto topHeight = m_core->getTopBlockIndex();

    try
    {
        generateBlockHeader(hash, writer);

        res.body = sb.GetString();

        return {SUCCESS, 200};
    }
    catch (const std::exception &)
    {
        return {Error(API_HASH_NOT_FOUND), 404};
    }
}

std::tuple<Error, uint16_t> RpcServer::getBlockHeaderByHeight(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    uint64_t height = 0;

    const auto topHeight = m_core->getTopBlockIndex();

    try
    {
        std::string heightStr = req.path.substr(std::string("/block/").size());

        height = std::stoull(heightStr);

        /* We cannot request a block height higher than the current top block */
        if (height > topHeight)
        {
            return {Error(API_INVALID_ARGUMENT, "Requested height cannot be greater than the top block height."), 400};
        }
    }
    catch (const std::out_of_range &)
    {
        return {Error(API_INVALID_ARGUMENT), 400};
    }
    catch (const std::invalid_argument &)
    {
        return {Error(API_INVALID_ARGUMENT), 400};
    }

    try
    {
        const auto hash = m_core->getBlockHashByIndex(height);

        generateBlockHeader(hash, writer);

        res.body = sb.GetString();

        return {SUCCESS, 200};
    }
    catch (const std::exception &)
    {
        return {Error(API_HASH_NOT_FOUND), 404};
    }
}

std::tuple<Error, uint16_t>
    RpcServer::getBlocksByHeight(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    std::string heightStr = req.path.substr(std::string("/block/headers/").size());

    uint64_t height;

    const auto topHeight = m_core->getTopBlockIndex();

    try
    {
        height = std::stoull(heightStr);

        /* We cannot request a block height higher than the current top block */
        if (height > topHeight)
        {
            return {Error(API_INVALID_ARGUMENT, "Requested height cannot be greater than the top block height."), 400};
        }
    }
    catch (const std::out_of_range &)
    {
        return {Error(API_INVALID_ARGUMENT), 400};
    }
    catch (const std::invalid_argument &)
    {
        return {Error(API_INVALID_ARGUMENT), 400};
    }

    const uint64_t MAX_BLOCKS_COUNT = 30;

    const uint64_t startHeight = height < MAX_BLOCKS_COUNT ? 0 : height - MAX_BLOCKS_COUNT;

    writer.StartArray();
    {
        /* Loop through the blocks in descending order and throw their resulting
         * headers into the array for the response */
        for (uint64_t i = height; i >= startHeight; i--)
        {
            const auto hash = m_core->getBlockHashByIndex(i);

            generateBlockHeader(hash, writer);
        }
    }
    writer.EndArray();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

void RpcServer::generateTransactionPrefix(
    const CryptoNote::Transaction &transaction,
    rapidjson::Writer<rapidjson::StringBuffer> &writer)
{
    writer.StartObject();
    {
        writer.Key("extra");
        writer.String(Common::toHex(transaction.extra));

        writer.Key("inputs");
        writer.StartArray();
        {
            for (const auto &input : transaction.inputs)
            {
                const auto type = input.type() == typeid(CryptoNote::BaseInput) ? "ff" : "02";

                writer.StartObject();
                {
                    if (input.type() == typeid(CryptoNote::BaseInput))
                    {
                        writer.Key("height");
                        writer.Uint64(boost::get<CryptoNote::BaseInput>(input).blockIndex);
                    }
                    else
                    {
                        const auto keyInput = boost::get<CryptoNote::KeyInput>(input);

                        writer.Key("amount");
                        writer.Uint64(keyInput.amount);

                        writer.Key("keyImage");
                        keyInput.keyImage.toJSON(writer);

                        writer.Key("offsets");
                        writer.StartArray();
                        {
                            for (const auto index : keyInput.outputIndexes)
                            {
                                writer.Uint(index);
                            }
                        }
                        writer.EndArray();
                    }

                    writer.Key("type");
                    writer.String(type);
                }
                writer.EndObject();
            }
        }
        writer.EndArray();

        writer.Key("outputs");
        writer.StartArray();
        {
            for (const auto &output : transaction.outputs)
            {
                writer.StartObject();
                {
                    writer.Key("amount");
                    writer.Uint64(output.amount);

                    writer.Key("key");
                    const auto key = boost::get<CryptoNote::KeyOutput>(output.target).key;
                    key.toJSON(writer);

                    writer.Key("type");
                    writer.String("02");
                }
                writer.EndObject();
            }
        }
        writer.EndArray();

        writer.Key("unlockTime");
        writer.Uint64(transaction.unlockTime);

        writer.Key("version");
        writer.Uint64(transaction.version);
    }
    writer.EndObject();
}

std::tuple<Error, uint16_t> RpcServer::getTransactionDetailsByHash(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    const std::string hashStr = req.path.substr(std::string("/transaction/").size());

    Crypto::Hash hash;

    if (!Common::podFromHex(hashStr, hash))
    {
        return {Error(API_INVALID_ARGUMENT), 400};
    }

    std::vector<Crypto::Hash> ignore;

    std::vector<std::vector<uint8_t>> rawTXs;

    std::vector<Crypto::Hash> hashes {hash};

    m_core->getTransactions(hashes, rawTXs, ignore);

    /* If we did not get exactly one transaction back then it's as if
     * we didn't get any transactions at all */
    if (rawTXs.size() != 1)
    {
        return {Error(API_HASH_NOT_FOUND), 404};
    }

    CryptoNote::Transaction transaction;

    CryptoNote::TransactionDetails txDetails = m_core->getTransactionDetails(hash);

    const uint64_t blockHeight = txDetails.blockIndex;

    const auto blockHash = m_core->getBlockHashByIndex(blockHeight);

    fromBinaryArray(transaction, rawTXs[0]);

    writer.StartObject();
    {
        /* This is a block header */
        writer.Key("block");
        generateBlockHeader(blockHash, writer, true);

        writer.Key("prefix");
        generateTransactionPrefix(transaction, writer);

        writer.Key("meta");
        writer.StartObject();
        {
            writer.Key("amountOut");
            writer.Uint64(txDetails.totalOutputsAmount);

            writer.Key("fee");
            writer.Uint64(txDetails.fee);

            writer.Key("paymentId");
            if (txDetails.paymentId == Constants::NULL_HASH)
            {
                writer.String("");
            }
            else
            {
                txDetails.paymentId.toJSON(writer);
            }

            writer.Key("publicKey");
            txDetails.extra.publicKey.toJSON(writer);

            writer.Key("ringSize");
            writer.Uint64(txDetails.mixin);

            writer.Key("size");
            writer.Uint64(txDetails.size);
        }
        writer.EndObject();
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getTransactionsInPool(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartArray();
    {
        for (const auto &tx : m_core->getPoolTransactions())
        {
            writer.StartObject();

            const uint64_t outputAmount =
                std::accumulate(tx.outputs.begin(), tx.outputs.end(), 0ull, [](const auto acc, const auto out) {
                    return acc + out.amount;
                });

            const uint64_t inputAmount =
                std::accumulate(tx.inputs.begin(), tx.inputs.end(), 0ull, [](const auto acc, const auto in) {
                    if (in.type() == typeid(CryptoNote::KeyInput))
                    {
                        return acc + boost::get<CryptoNote::KeyInput>(in).amount;
                    }

                    return acc;
                });

            const uint64_t fee = inputAmount - outputAmount;

            writer.Key("amountOut");
            writer.Uint64(outputAmount);

            writer.Key("fee");
            writer.Uint64(fee);

            writer.Key("hash");
            const auto txHash = getObjectHash(tx);
            txHash.toJSON(writer);

            writer.Key("size");
            writer.Uint64(getObjectBinarySize(tx));

            writer.EndObject();
        }
    }
    writer.EndArray();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getTransactionsStatus(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    std::unordered_set<Crypto::Hash> transactionHashes;

    for (const auto &hashStr : getArrayFromJSON(body))
    {
        Crypto::Hash hash;

        if (!Common::podFromHex(getStringFromJSON(hashStr), hash))
        {
            return {Error(API_INVALID_ARGUMENT), 400};
        }

        transactionHashes.insert(hash);
    }

    std::unordered_set<Crypto::Hash> transactionsInPool;

    std::unordered_set<Crypto::Hash> transactionsInBlock;

    std::unordered_set<Crypto::Hash> transactionsUnknown;

    const bool success =
        m_core->getTransactionsStatus(transactionHashes, transactionsInPool, transactionsInBlock, transactionsUnknown);

    if (!success)
    {
        return {Error(API_INTERNAL_ERROR, "Could not retrieve transactions status."), 500};
    }

    writer.StartObject();
    {
        writer.Key("inBlock");
        writer.StartArray();
        {
            for (const auto &hash : transactionsInBlock)
            {
                hash.toJSON(writer);
            }
        }
        writer.EndArray();

        writer.Key("inPool");
        writer.StartArray();
        {
            for (const auto &hash : transactionsInPool)
            {
                hash.toJSON(writer);
            }
        }
        writer.EndArray();

        writer.Key("notFound");
        writer.StartArray();
        {
            for (const auto &hash : transactionsUnknown)
            {
                hash.toJSON(writer);
            }
        }
        writer.EndArray();
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t>
    RpcServer::getPoolChanges(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    Crypto::Hash lastBlockHash;

    if (!Common::podFromHex(getStringFromJSON(body, "lastKnownBlock"), lastBlockHash))
    {
        return {Error(API_INVALID_ARGUMENT), 400};
    }

    std::vector<Crypto::Hash> knownHashes;

    for (const auto &hashStr : getArrayFromJSON(body, "transactions"))
    {
        Crypto::Hash hash;

        if (!Common::podFromHex(getStringFromJSON(hashStr), hash))
        {
            return {Error(API_INVALID_ARGUMENT), 400};
        }

        knownHashes.push_back(hash);
    }

    std::vector<CryptoNote::Transaction> addedTransactions;

    std::vector<Crypto::Hash> deletedTransactions;

    const bool atTopOfChain =
        m_core->getPoolChangesLite(lastBlockHash, knownHashes, addedTransactions, deletedTransactions);

    writer.StartObject();
    {
        writer.Key("added");
        writer.StartArray();
        {
            for (const auto &transaction : addedTransactions)
            {
                const auto tx = CryptoNote::toBinaryArray(transaction);

                writer.String(Common::toHex(tx));
            }
        }
        writer.EndArray();

        writer.Key("deleted");
        writer.StartArray();
        {
            for (const auto hash : deletedTransactions)
            {
                hash.toJSON(writer);
            }
        }
        writer.EndArray();

        writer.Key("synced");
        writer.Bool(atTopOfChain);
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t>
    RpcServer::getRawBlocks(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        std::vector<Crypto::Hash> blockHashCheckpoints;

        if (hasMember(body, "checkpoints"))
        {
            for (const auto &jsonHash : getArrayFromJSON(body, "checkpoints"))
            {
                std::string hashStr = jsonHash.GetString();

                Crypto::Hash hash;
                Common::podFromHex(hashStr, hash);

                blockHashCheckpoints.push_back(hash);
            }
        }

        const uint64_t startHeight = hasMember(body, "height") ? getUint64FromJSON(body, "height") : 0;

        const uint64_t startTimestamp = hasMember(body, "timestamp") ? getUint64FromJSON(body, "timestamp") : 0;

        const uint64_t blockCount = hasMember(body, "count") ? getUint64FromJSON(body, "count") : 100;

        const bool skipCoinbaseTransactions =
            hasMember(body, "skipCoinbaseTransactions") ? getBoolFromJSON(body, "skipCoinbaseTransactions") : false;

        std::vector<CryptoNote::RawBlock> rawBlocks;

        std::optional<WalletTypes::TopBlock> topBlockInfo;

        const bool success = m_core->getRawBlocks(
            blockHashCheckpoints,
            startHeight,
            startTimestamp,
            blockCount,
            skipCoinbaseTransactions,
            rawBlocks,
            topBlockInfo);

        if (!success)
        {
            return {Error(API_INTERNAL_ERROR, "Failed to retrieve raw blocks from underlying storage."), 500};
        }

        writer.Key("blocks");
        writer.StartArray();
        {
            for (const auto &rawBlock : rawBlocks)
            {
                rawBlock.toJSON(writer);
            }
        }
        writer.EndArray();

        writer.Key("synced");
        writer.Bool(rawBlocks.empty());

        if (topBlockInfo)
        {
            writer.Key("topBlock");
            writer.StartObject();
            {
                writer.Key("hash");
                topBlockInfo->hash.toJSON(writer);

                writer.Key("height");
                writer.Uint64(topBlockInfo->height);
            }
            writer.EndObject();
        }
    }
    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t>
    RpcServer::getRawBlockByHash(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    std::string stripped = req.path.substr(std::string("/block/").size());

    uint64_t splitPos = stripped.find_first_of("/");

    std::string hashStr = stripped.substr(0, splitPos);

    Crypto::Hash hash;

    if (!Common::podFromHex(hashStr, hash))
    {
        return {Error(API_INVALID_ARGUMENT), 400};
    }

    try
    {
        const CryptoNote::RawBlock rawBlock = m_core->getRawBlock(hash);

        rawBlock.toJSON(writer);

        res.body = sb.GetString();

        return {SUCCESS, 200};
    }
    catch (const std::exception &)
    {
        return {SUCCESS, 404};
    }
}

std::tuple<Error, uint16_t>
    RpcServer::getRawBlockByHeight(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    std::string stripped = req.path.substr(std::string("/block/").size());

    uint64_t splitPos = stripped.find_first_of("/");

    std::string heightStr = stripped.substr(0, splitPos);

    uint32_t height = 0;

    const auto topHeight = m_core->getTopBlockIndex();

    try
    {
        height = std::stoull(heightStr);

        if (height > topHeight)
        {
            return {Error(API_INVALID_ARGUMENT, "Requested height cannot be greater than the top block height."), 400};
        }
    }
    catch (const std::out_of_range &)
    {
        return {Error(API_INVALID_ARGUMENT), 400};
    }
    catch (const std::invalid_argument &)
    {
        return {Error(API_INVALID_ARGUMENT), 400};
    }

    try
    {
        const CryptoNote::RawBlock rawBlock = m_core->getRawBlock(height);

        rawBlock.toJSON(writer);

        res.body = sb.GetString();

        return {SUCCESS, 200};
    }
    catch (const std::exception &)
    {
        return {Error(API_HASH_NOT_FOUND), 404};
    }
}

std::tuple<Error, uint16_t> RpcServer::getRawTransactionByHash(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    std::string stripped = req.path.substr(std::string("/transaction/").size());

    uint64_t splitPos = stripped.find_first_of("/");

    std::string hashStr = stripped.substr(0, splitPos);

    Crypto::Hash hash;

    if (!Common::podFromHex(hashStr, hash))
    {
        return {Error(API_INVALID_ARGUMENT), 400};
    }

    const auto transaction = m_core->getTransaction(hash);

    if (!transaction.has_value())
    {
        return {Error(API_HASH_NOT_FOUND), 404};
    }

    writer.String(Common::toHex(transaction.value()));

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getRawTransactionsInPool(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartArray();
    {
        for (const auto &tx : m_core->getPoolTransactions())
        {
            const auto transaction = toBinaryArray(tx);

            writer.String(Common::toHex(transaction));
        }
    }
    writer.EndArray();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}