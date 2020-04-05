// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

//////////////////////////
#include <rpc/RpcServer.h>
//////////////////////////

#include <iostream>

#include "version.h"

#include <config/Constants.h>
#include <common/CryptoNoteTools.h>
#include <errors/ValidateParameters.h>
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
    const auto router = [this](const auto function, const RpcMode routePermissions, const bool isBodyRequired, const bool syncRequired) {
        return [=](const httplib::Request &req, httplib::Response &res) {
            /* Pass the inputted function with the arguments passed through
               to middleware */
            middleware(
                req,
                res,
                routePermissions,
                isBodyRequired,
                syncRequired,
                std::bind(function, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
            );
        };
    };

    const auto jsonRpc = [this, router, bodyRequired, bodyNotRequired, syncRequired, syncNotRequired](const auto &req, auto &res) {
        const auto body = getJsonBody(req, res, true);

        if (!body)
        {
            return;
        }

        if (!hasMember(*body, "method"))
        {
            failRequest(400, "Missing JSON parameter: 'method'", res);
            return;
        }

        const auto method = getStringFromJSON(*body, "method");

        if (method == "getblocktemplate")
        {
            router(&RpcServer::getBlockTemplate, RpcMode::Default, bodyRequired, syncRequired)(req, res);
        }
        else if (method == "submitblock")
        {
            router(&RpcServer::submitBlock, RpcMode::Default, bodyRequired, syncRequired)(req, res);
        }
        else if (method == "getblockcount")
        {
            router(&RpcServer::getBlockCount, RpcMode::Default, bodyNotRequired, syncNotRequired)(req, res);
        }
        else if (method == "getlastblockheader")
        {
            router(&RpcServer::getLastBlockHeader, RpcMode::Default, bodyNotRequired, syncNotRequired)(req, res);
        }
        else if (method == "getblockheaderbyhash")
        {
            router(&RpcServer::getBlockHeaderByHash, RpcMode::Default, bodyRequired, syncNotRequired)(req, res);
        }
        else if (method == "getblockheaderbyheight")
        {
            router(&RpcServer::getBlockHeaderByHeight, RpcMode::Default, bodyRequired, syncNotRequired)(req, res);
        }
        else if (method == "f_blocks_list_json")
        {
            router(&RpcServer::getBlocksByHeight, RpcMode::BlockExplorerEnabled, bodyRequired, syncNotRequired)(req, res);
        }
        else if (method == "f_block_json")
        {
            router(&RpcServer::getBlockDetailsByHash, RpcMode::BlockExplorerEnabled, bodyRequired, syncNotRequired)(req, res);
        }
        else if (method == "f_transaction_json")
        {
            router(&RpcServer::getTransactionDetailsByHash, RpcMode::BlockExplorerEnabled, bodyRequired, syncNotRequired)(req, res);
        }
        else if (method == "f_on_transactions_pool_json")
        {
            router(&RpcServer::getTransactionsInPool, RpcMode::BlockExplorerEnabled, bodyNotRequired, syncNotRequired)(req, res);
        }
        else
        {
            res.status = 404;
        }
    };

    /* Note: /json_rpc is exposed on both GET and POST */
    m_server.Get("/json_rpc", jsonRpc)
            .Get("/info", router(&RpcServer::info, RpcMode::Default, bodyNotRequired, syncNotRequired))
            .Get("/fee", router(&RpcServer::fee, RpcMode::Default, bodyNotRequired, syncNotRequired))
            .Get("/height", router(&RpcServer::height, RpcMode::Default, bodyNotRequired, syncNotRequired))
            .Get("/peers", router(&RpcServer::peers, RpcMode::Default, bodyNotRequired, syncNotRequired))

            .Post("/json_rpc", jsonRpc)
            .Post("/sendrawtransaction", router(&RpcServer::sendTransaction, RpcMode::Default, bodyRequired, syncRequired))
            .Post("/getrandom_outs", router(&RpcServer::getRandomOuts, RpcMode::Default, bodyRequired, syncNotRequired))
            .Post("/getwalletsyncdata", router(&RpcServer::getWalletSyncData, RpcMode::Default, bodyRequired, syncNotRequired))
            .Post("/get_global_indexes_for_range", router(&RpcServer::getGlobalIndexes, RpcMode::Default, bodyRequired, syncNotRequired))
            .Post("/queryblockslite", router(&RpcServer::queryBlocksLite, RpcMode::Default, bodyRequired, syncNotRequired))
            .Post("/get_transactions_status", router(&RpcServer::getTransactionsStatus, RpcMode::Default, bodyRequired, syncNotRequired))
            .Post("/get_pool_changes_lite", router(&RpcServer::getPoolChanges, RpcMode::Default, bodyRequired, syncNotRequired))
            .Post("/queryblocksdetailed", router(&RpcServer::queryBlocksDetailed, RpcMode::AllMethodsEnabled, bodyRequired, syncNotRequired))
            .Post("/get_o_indexes", router(&RpcServer::getGlobalIndexesDeprecated, RpcMode::Default, bodyRequired, syncNotRequired))
            .Post("/getrawblocks", router(&RpcServer::getRawBlocks, RpcMode::Default, bodyRequired, syncNotRequired))

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

std::optional<rapidjson::Document> RpcServer::getJsonBody(
    const httplib::Request &req,
    httplib::Response &res,
    const bool bodyRequired)
{
    rapidjson::Document jsonBody;

    if (!bodyRequired)
    {
        /* Some compilers are stupid and can't figure out just `return jsonBody`
         * and we can't construct a std::optional(jsonBody) since the copy
         * constructor is deleted, so we need to std::move */
        return std::optional<rapidjson::Document>(std::move(jsonBody));
    }

    if (jsonBody.Parse(req.body.c_str()).HasParseError())
    {
        std::stringstream stream;

        if (!req.body.empty())
        {
            stream << "Warning: received body is not JSON encoded!\n"
                   << "Key/value parameters are NOT supported.\n"
                   << "Body:\n" << req.body;

            Logger::logger.log(
                stream.str(),
                Logger::INFO,
                { Logger::DAEMON_RPC }
            );
        }

        stream << "Failed to parse request body as JSON";

        failRequest(400, stream.str(), res);

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
        "[" + req.get_header_value("REMOTE_ADDR") + "] Incoming " + req.method + " request: " + req.path + ", User-Agent: " + req.get_header_value("User-Agent"),
        Logger::DEBUG,
        { Logger::DAEMON_RPC }
    );

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
        std::stringstream stream;

        stream << "You do not have permission to access this method. Please "
                  "relaunch your daemon with the --enable-blockexplorer";

        if (routePermissions == RpcMode::AllMethodsEnabled)
        {
            stream << "-detailed";
        }

        stream << " command line option to access this method.";

        failRequest(403, stream.str(), res);

        return;
    }

    const uint64_t height = m_core->getTopBlockIndex() + 1;
    const uint64_t networkHeight = std::max(1u, m_syncManager->getBlockchainHeight());

    const bool areSynced = m_p2p->get_payload_object().isSynchronized() && height >= networkHeight;

    if (syncRequired && !areSynced)
    {
        failRequest(200, "Daemon must be synced to process this RPC method call, please retry when synced", res);
        return;
    }

    try
    {
        const auto [error, statusCode] = handler(req, res, *jsonBody);

        if (error)
        {
            rapidjson::StringBuffer sb;
            rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

            writer.StartObject();

            writer.Key("errorCode");
            writer.Uint(error.getErrorCode());

            writer.Key("errorMessage");
            writer.String(error.getErrorMessage());

            writer.EndObject();

            res.body = sb.GetString();
            res.status = 400;
        }
        else
        {
            res.status = statusCode;
        }

        return;
    }
    catch (const std::invalid_argument &e)
    {
        Logger::logger.log(
            "Caught JSON exception, likely missing required json parameter: " + std::string(e.what()),
            Logger::FATAL,
            { Logger::DAEMON_RPC }
        );

        failRequest(400, e.what(), res);
    }
    catch (const std::exception &e)
    {
        std::stringstream error;

        error << "Caught unexpected exception: " << e.what() << " while processing "
              << req.path << " request for User-Agent: " << req.get_header_value("User-Agent");

        Logger::logger.log(
            error.str(),
            Logger::FATAL,
            { Logger::DAEMON_RPC }
        );

        if (req.body != "")
        {
            Logger::logger.log(
                "Body: " + req.body,
                Logger::FATAL,
                { Logger::DAEMON_RPC }
            );
        }

        failRequest(500, "Internal server error: " + std::string(e.what()), res);
    }
}

void RpcServer::failRequest(uint16_t statusCode, std::string body, httplib::Response &res)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();

    writer.Key("status");
    writer.String("Failed");

    writer.Key("error");
    writer.String(body);

    writer.EndObject();

    res.body = sb.GetString();
    res.status = statusCode;
}

void RpcServer::failJsonRpcRequest(
    const int64_t errorCode,
    const std::string errorMessage,
    httplib::Response &res)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("jsonrpc");
        writer.String("2.0");

        writer.Key("error");
        writer.StartObject();
        {
            writer.Key("message");
            writer.String(errorMessage);

            writer.Key("code");
            writer.Int64(errorCode);
        }
        writer.EndObject();
    }
    writer.EndObject();

    res.body = sb.GetString();
    res.status = 200;
}

void RpcServer::handleOptions(const httplib::Request &req, httplib::Response &res) const
{
    Logger::logger.log(
        "Incoming " + req.method + " request: " + req.path,
        Logger::DEBUG,
        { Logger::DAEMON_RPC }
    );

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

std::tuple<Error, uint16_t> RpcServer::info(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    const uint64_t height = m_core->getTopBlockIndex() + 1;
    const uint64_t networkHeight = std::max(1u, m_syncManager->getBlockchainHeight());
    const auto blockDetails = m_core->getBlockDetails(height - 1);
    const uint64_t difficulty = m_core->getDifficultyForNextBlock();

    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();

    writer.Key("height");
    writer.Uint64(height);

    writer.Key("difficulty");
    writer.Uint64(difficulty);

    writer.Key("tx_count");
    /* Transaction count without coinbase transactions - one per block, so subtract height */
    writer.Uint64(m_core->getBlockchainTransactionCount() - height);

    writer.Key("tx_pool_size");
    writer.Uint64(m_core->getPoolTransactionCount());

    writer.Key("alt_blocks_count");
    writer.Uint64(m_core->getAlternativeBlockCount());

    uint64_t total_conn = m_p2p->get_connections_count();
    uint64_t outgoing_connections_count = m_p2p->get_outgoing_connections_count();

    writer.Key("outgoing_connections_count");
    writer.Uint64(outgoing_connections_count);

    writer.Key("incoming_connections_count");
    writer.Uint64(total_conn - outgoing_connections_count);

    writer.Key("white_peerlist_size");
    writer.Uint64(m_p2p->getPeerlistManager().get_white_peers_count());

    writer.Key("grey_peerlist_size");
    writer.Uint64(m_p2p->getPeerlistManager().get_gray_peers_count());

    writer.Key("last_known_block_index");
    writer.Uint64(std::max(1u, m_syncManager->getObservedHeight()) - 1);

    writer.Key("network_height");
    writer.Uint64(networkHeight);

    writer.Key("upgrade_heights");
    writer.StartArray();
    {
        for (const uint64_t height : CryptoNote::parameters::FORK_HEIGHTS)
        {
            writer.Uint64(height);
        }
    }
    writer.EndArray();

    writer.Key("supported_height");
    writer.Uint64(CryptoNote::parameters::FORK_HEIGHTS_SIZE == 0
        ? 0
        : CryptoNote::parameters::FORK_HEIGHTS[CryptoNote::parameters::CURRENT_FORK_INDEX]);

    writer.Key("hashrate");
    writer.Uint64(round(difficulty / CryptoNote::parameters::DIFFICULTY_TARGET));

    writer.Key("synced");
    writer.Bool(height == networkHeight);

    writer.Key("major_version");
    writer.Uint64(blockDetails.majorVersion);

    writer.Key("minor_version");
    writer.Uint64(blockDetails.minorVersion);

    writer.Key("version");
    writer.String(PROJECT_VERSION);

    writer.Key("status");
    writer.String("OK");

    writer.Key("start_time");
    writer.Uint64(m_core->getStartTime());

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::fee(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();

    writer.Key("address");
    writer.String(m_feeAddress);

    writer.Key("amount");
    writer.Uint64(m_feeAmount);

    writer.Key("status");
    writer.String("OK");

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::height(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();

    writer.Key("height");
    writer.Uint64(m_core->getTopBlockIndex() + 1);

    writer.Key("network_height");
    writer.Uint64(std::max(1u, m_syncManager->getBlockchainHeight()));

    writer.Key("status");
    writer.String("OK");

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::peers(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();

    std::list<PeerlistEntry> peers_white;
    std::list<PeerlistEntry> peers_gray;

    m_p2p->getPeerlistManager().get_peerlist_full(peers_gray, peers_white);

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

    writer.Key("peers_gray");
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

    writer.Key("status");
    writer.String("OK");

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::sendTransaction(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    std::vector<uint8_t> transaction;

    const std::string rawData = getStringFromJSON(body, "tx_as_hex");

    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();

    if (!Common::fromHex(rawData, transaction))
    {
        writer.Key("status");
        writer.String("Failed");

        writer.Key("error");
        writer.String("Failed to parse transaction from hex buffer");
    }
    else
    {
        Crypto::Hash transactionHash = Crypto::cn_fast_hash(transaction.data(), transaction.size());

        writer.Key("transactionHash");
        writer.String(Common::podToHex(transactionHash));

        std::stringstream stream;

        stream << "Attempting to add transaction " << transactionHash << " from /sendrawtransaction to pool";

        Logger::logger.log(
            stream.str(),
            Logger::DEBUG,
            { Logger::DAEMON_RPC }
        );

        const auto [success, error] = m_core->addTransactionToPool(transaction);

        if (!success)
        {
            /* Empty stream */
            std::stringstream().swap(stream);

            stream << "Failed to add transaction " << transactionHash << " from /sendrawtransaction to pool: " << error;

            Logger::logger.log(
                stream.str(),
                Logger::INFO,
                { Logger::DAEMON_RPC }
            );

            writer.Key("status");
            writer.String("Failed");

            writer.Key("error");
            writer.String(error);
        }
        else
        {
            m_syncManager->relayTransactions({transaction});

            writer.Key("status");
            writer.String("OK");

            writer.Key("error");
            writer.String("");

        }
    }

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getRandomOuts(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    const uint64_t numOutputs = getUint64FromJSON(body, "outs_count");

    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();

    writer.Key("outs");

    writer.StartArray();
    {
        for (const auto &jsonAmount : getArrayFromJSON(body, "amounts"))
        {
            writer.StartObject();

            const uint64_t amount = jsonAmount.GetUint64();

            std::vector<uint32_t> globalIndexes;
            std::vector<Crypto::PublicKey> publicKeys;

            const auto [success, error] = m_core->getRandomOutputs(
                amount, static_cast<uint16_t>(numOutputs), globalIndexes, publicKeys
            );

            if (!success)
            {
                return {Error(CANT_GET_FAKE_OUTPUTS, error), 200};
            }

            if (globalIndexes.size() != numOutputs)
            {
                std::stringstream stream;

                stream << "Failed to get enough matching outputs for amount " << amount << " ("
                       << Utilities::formatAmount(amount) << "). Requested outputs: " << numOutputs
                       << ", found outputs: " << globalIndexes.size()
                       << ". Further explanation here: https://gist.github.com/zpalmtree/80b3e80463225bcfb8f8432043cb594c"
                       << std::endl
                       << "Note: If you are a public node operator, you can safely ignore this message. "
                       << "It is only relevant to the user sending the transaction.";

                return {Error(CANT_GET_FAKE_OUTPUTS, stream.str()), 200};
            }

            writer.Key("amount");
            writer.Uint64(amount);

            writer.Key("outs");
            writer.StartArray();
            {
                for (size_t i = 0; i < globalIndexes.size(); i++)
                {
                    writer.StartObject();
                    {
                        writer.Key("global_amount_index");
                        writer.Uint64(globalIndexes[i]);

                        writer.Key("out_key");
                        writer.String(Common::podToHex(publicKeys[i]));
                    }
                    writer.EndObject();
                }
            }
            writer.EndArray();

            writer.EndObject();
        }
    }
    writer.EndArray();

    writer.Key("status");
    writer.String("OK");

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getWalletSyncData(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();

    std::vector<Crypto::Hash> blockHashCheckpoints;

    if (hasMember(body, "blockHashCheckpoints"))
    {
        for (const auto &jsonHash : getArrayFromJSON(body, "blockHashCheckpoints"))
        {
            std::string hashStr = jsonHash.GetString();

            Crypto::Hash hash;
            Common::podFromHex(hashStr, hash);

            blockHashCheckpoints.push_back(hash);
        }
    }

    const uint64_t startHeight = hasMember(body, "startHeight")
        ? getUint64FromJSON(body, "startHeight")
        : 0;

    const uint64_t startTimestamp = hasMember(body, "startTimestamp")
        ? getUint64FromJSON(body, "startTimestamp")
        : 0;

    const uint64_t blockCount = hasMember(body, "blockCount")
        ? getUint64FromJSON(body, "blockCount")
        : 100;

    const bool skipCoinbaseTransactions = hasMember(body, "skipCoinbaseTransactions")
        ? getBoolFromJSON(body, "skipCoinbaseTransactions")
        : false;

    std::vector<WalletTypes::WalletBlockInfo> walletBlocks;
    std::optional<WalletTypes::TopBlock> topBlockInfo;

    const bool success = m_core->getWalletSyncData(
        blockHashCheckpoints,
        startHeight,
        startTimestamp,
        blockCount,
        skipCoinbaseTransactions,
        walletBlocks,
        topBlockInfo
    );

    if (!success)
    {
        return {SUCCESS, 500};
    }

    writer.Key("items");
    writer.StartArray();
    {
        for (const auto &block : walletBlocks)
        {
            writer.StartObject();

            if (block.coinbaseTransaction)
            {
                writer.Key("coinbaseTX");
                writer.StartObject();
                {
                    writer.Key("outputs");
                    writer.StartArray();
                    {
                        for (const auto &output : block.coinbaseTransaction->keyOutputs)
                        {
                            writer.StartObject();
                            {
                                writer.Key("key");
                                writer.String(Common::podToHex(output.key));

                                writer.Key("amount");
                                writer.Uint64(output.amount);
                            }
                            writer.EndObject();
                        }
                    }
                    writer.EndArray();

                    writer.Key("hash");
                    writer.String(Common::podToHex(block.coinbaseTransaction->hash));

                    writer.Key("txPublicKey");
                    writer.String(Common::podToHex(block.coinbaseTransaction->transactionPublicKey));

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
                        writer.Key("outputs");
                        writer.StartArray();
                        {
                            for (const auto &output : transaction.keyOutputs)
                            {
                                writer.StartObject();
                                {
                                    writer.Key("key");
                                    writer.String(Common::podToHex(output.key));

                                    writer.Key("amount");
                                    writer.Uint64(output.amount);
                                }
                                writer.EndObject();
                            }
                        }
                        writer.EndArray();

                        writer.Key("hash");
                        writer.String(Common::podToHex(transaction.hash));

                        writer.Key("txPublicKey");
                        writer.String(Common::podToHex(transaction.transactionPublicKey));

                        writer.Key("unlockTime");
                        writer.Uint64(transaction.unlockTime);

                        writer.Key("paymentID");
                        writer.String(transaction.paymentID);

                        writer.Key("inputs");
                        writer.StartArray();
                        {
                            for (const auto &input : transaction.keyInputs)
                            {
                                writer.StartObject();
                                {
                                    writer.Key("amount");
                                    writer.Uint64(input.amount);

                                    writer.Key("key_offsets");
                                    writer.StartArray();
                                    {
                                        for (const auto &offset : input.outputIndexes)
                                        {
                                            writer.Uint64(offset);
                                        }
                                    }
                                    writer.EndArray();

                                    writer.Key("k_image");
                                    writer.String(Common::podToHex(input.keyImage));
                                }
                                writer.EndObject();
                            }
                        }
                        writer.EndArray();
                    }
                    writer.EndObject();
                }
            }
            writer.EndArray();

            writer.Key("blockHeight");
            writer.Uint64(block.blockHeight);

            writer.Key("blockHash");
            writer.String(Common::podToHex(block.blockHash));

            writer.Key("blockTimestamp");
            writer.Uint64(block.blockTimestamp);

            writer.EndObject();
        }
    }
    writer.EndArray();

    if (topBlockInfo)
    {
        writer.Key("topBlock");
        writer.StartObject();
        {
            writer.Key("hash");
            writer.String(Common::podToHex(topBlockInfo->hash));

            writer.Key("height");
            writer.Uint64(topBlockInfo->height);
        }
        writer.EndObject();
    }

    writer.Key("synced");
    writer.Bool(walletBlocks.empty());

    writer.Key("status");
    writer.String("OK");

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getGlobalIndexes(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    const uint64_t startHeight = getUint64FromJSON(body, "startHeight");
    const uint64_t endHeight = getUint64FromJSON(body, "endHeight");

    std::unordered_map<Crypto::Hash, std::vector<uint64_t>> indexes;

    const bool success = m_core->getGlobalIndexesForRange(startHeight, endHeight, indexes);

    writer.StartObject();

    if (!success)
    {
        writer.Key("status");
        writer.String("Failed");

        res.body = sb.GetString();

        return {SUCCESS, 500};
    }

    writer.Key("indexes");

    writer.StartArray();
    {
        for (const auto [hash, globalIndexes] : indexes)
        {
            writer.StartObject();

            writer.Key("key");
            writer.String(Common::podToHex(hash));

            writer.Key("value");
            writer.StartArray();
            {
                for (const auto index : globalIndexes)
                {
                    writer.Uint64(index);
                }
            }
            writer.EndArray();

            writer.EndObject();
        }
    }
    writer.EndArray();

    writer.Key("status");
    writer.String("OK");

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getBlockTemplate(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    const auto params = getObjectFromJSON(body, "params");

    writer.StartObject();

    const uint64_t reserveSize = getUint64FromJSON(params, "reserve_size");

    if (reserveSize > 255)
    {
        failJsonRpcRequest(
            -3,
            "Too big reserved size, maximum allowed is 255",
            res
        );

        return {SUCCESS, 200};
    }

    const std::string address = getStringFromJSON(params, "wallet_address");

    Error addressError = validateAddresses({address}, false);

    if (addressError)
    {
        failJsonRpcRequest(
            -4,
            addressError.getErrorMessage(),
            res
        );

        return {SUCCESS, 200};
    }

    const auto [publicSpendKey, publicViewKey] = Utilities::addressToKeys(address);

    CryptoNote::BlockTemplate blockTemplate;

    std::vector<uint8_t> blobReserve;
    blobReserve.resize(reserveSize, 0);

    uint64_t difficulty;
    uint32_t height;

    const auto [success, error] = m_core->getBlockTemplate(
        blockTemplate, publicViewKey, publicSpendKey, blobReserve, difficulty, height
    );

    if (!success)
    {
        failJsonRpcRequest(
            -5,
            "Failed to create block template: " + error,
            res
        );

        return {SUCCESS, 200};
    }

    std::vector<uint8_t> blockBlob = CryptoNote::toBinaryArray(blockTemplate);

    const auto transactionPublicKey = Utilities::getTransactionPublicKeyFromExtra(
        blockTemplate.baseTransaction.extra
    );

    uint64_t reservedOffset = 0;

    if (reserveSize > 0)
    {
        /* Find where in the block blob the transaction public key is */
        const auto it = std::search(
            blockBlob.begin(),
            blockBlob.end(),
            std::begin(transactionPublicKey.data),
            std::end(transactionPublicKey.data)
        );

        /* The reserved offset is past the transactionPublicKey, then past
         * the extra nonce tags */
        reservedOffset = (it - blockBlob.begin()) + sizeof(transactionPublicKey) + 2;

        if (reservedOffset + reserveSize > blockBlob.size())
        {
            failJsonRpcRequest(
                -5,
                "Internal error: failed to create block template, not enough space for reserved bytes",
                res
            );

            return {SUCCESS, 200};
        }
    }

    writer.Key("jsonrpc");
    writer.String("2.0");

    writer.Key("result");
    writer.StartObject();
    {
        writer.Key("height");
        writer.Uint(height);

        writer.Key("difficulty");
        writer.Uint64(difficulty);

        writer.Key("reserved_offset");
        writer.Uint64(reservedOffset);

        writer.Key("blocktemplate_blob");
        writer.String(Common::toHex(blockBlob));

        writer.Key("status");
        writer.String("OK");
    }
    writer.EndObject();

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::submitBlock(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    const auto params = getArrayFromJSON(body, "params");

    if (params.Size() != 1)
    {
        failJsonRpcRequest(
            -1,
            "You must submit one and only one block blob! (Found " + std::to_string(params.Size()) + ")",
            res
        );

        return {SUCCESS, 200};
    }

    const std::string blockBlob = getStringFromJSONString(params[0]);

    std::vector<uint8_t> rawBlob;

    if (!Common::fromHex(blockBlob, rawBlob))
    {
        failJsonRpcRequest(
            -6,
            "Submitted block blob is not hex!",
            res
        );

        return {SUCCESS, 200};
    }

    const auto submitResult = m_core->submitBlock(rawBlob);

    if (submitResult != CryptoNote::error::AddBlockErrorCondition::BLOCK_ADDED)
    {
        failJsonRpcRequest(
            -7,
            "Block not accepted",
            res
        );

        return {SUCCESS, 200};
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
    }

    writer.StartObject();

    writer.Key("jsonrpc");
    writer.String("2.0");

    writer.Key("result");
    writer.StartObject();
    {
        writer.Key("status");
        writer.String("OK");
    }
    writer.EndObject();

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getBlockCount(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();

    writer.Key("jsonrpc");
    writer.String("2.0");

    writer.Key("result");
    writer.StartObject();
    {
        writer.Key("status");
        writer.String("OK");

        writer.Key("count");
        writer.Uint64(m_core->getTopBlockIndex() + 1);
    }
    writer.EndObject();

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getLastBlockHeader(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    const auto height = m_core->getTopBlockIndex();
    const auto hash = m_core->getBlockHashByIndex(height);
    const auto topBlock = m_core->getBlockByHash(hash);
    const auto outputs = topBlock.baseTransaction.outputs;
    const auto extraDetails = m_core->getBlockDetails(hash);

    const uint64_t reward = std::accumulate(outputs.begin(), outputs.end(), 0ull,
        [](const auto acc, const auto out) {
            return acc + out.amount;
        }
    );

    writer.StartObject();

    writer.Key("jsonrpc");
    writer.String("2.0");

    writer.Key("result");
    writer.StartObject();
    {
        writer.Key("status");
        writer.String("OK");

        writer.Key("block_header");
        writer.StartObject();
        {
            writer.Key("major_version");
            writer.Uint64(topBlock.majorVersion);

            writer.Key("minor_version");
            writer.Uint64(topBlock.minorVersion);

            writer.Key("timestamp");
            writer.Uint64(topBlock.timestamp);

            writer.Key("prev_hash");
            writer.String(Common::podToHex(topBlock.previousBlockHash));

            writer.Key("nonce");
            writer.Uint64(topBlock.nonce);

            writer.Key("orphan_status");
            writer.Bool(extraDetails.isAlternative);

            writer.Key("height");
            writer.Uint64(height);

            writer.Key("depth");
            writer.Uint64(0);

            writer.Key("hash");
            writer.String(Common::podToHex(hash));

            writer.Key("difficulty");
            writer.Uint64(m_core->getBlockDifficulty(height));

            writer.Key("reward");
            writer.Uint64(reward);

            writer.Key("num_txes");
            writer.Uint64(extraDetails.transactions.size());

            writer.Key("block_size");
            writer.Uint64(extraDetails.blockSize);
        }
        writer.EndObject();
    }
    writer.EndObject();

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getBlockHeaderByHash(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    const auto params = getObjectFromJSON(body, "params");
    const auto hashStr = getStringFromJSON(params, "hash");
    const auto topHeight = m_core->getTopBlockIndex();

    Crypto::Hash hash;

    if (!Common::podFromHex(hashStr, hash))
    {
        failJsonRpcRequest(
            -1,
            "Block hash specified is not a valid hex!",
            res
        );

        return {SUCCESS, 200};
    }

    CryptoNote::BlockTemplate block;

    try
    {
        block = m_core->getBlockByHash(hash);
    }
    catch (const std::runtime_error &)
    {
        failJsonRpcRequest(
            -5,
            "Block hash specified does not exist!",
            res
        );

        return {SUCCESS, 200};
    }

    CryptoNote::CachedBlock cachedBlock(block);

    const auto height = cachedBlock.getBlockIndex();
    const auto outputs = block.baseTransaction.outputs;
    const auto extraDetails = m_core->getBlockDetails(hash);

    const uint64_t reward = std::accumulate(outputs.begin(), outputs.end(), 0ull,
        [](const auto acc, const auto out) {
            return acc + out.amount;
        }
    );

    writer.StartObject();

    writer.Key("jsonrpc");
    writer.String("2.0");

    writer.Key("result");
    writer.StartObject();
    {
        writer.Key("status");
        writer.String("OK");

        writer.Key("block_header");
        writer.StartObject();
        {
            writer.Key("major_version");
            writer.Uint64(block.majorVersion);

            writer.Key("minor_version");
            writer.Uint64(block.minorVersion);

            writer.Key("timestamp");
            writer.Uint64(block.timestamp);

            writer.Key("prev_hash");
            writer.String(Common::podToHex(block.previousBlockHash));

            writer.Key("nonce");
            writer.Uint64(block.nonce);

            writer.Key("orphan_status");
            writer.Bool(extraDetails.isAlternative);

            writer.Key("height");
            writer.Uint64(height);

            writer.Key("depth");
            writer.Uint64(topHeight - height);

            writer.Key("hash");
            writer.String(Common::podToHex(hash));

            writer.Key("difficulty");
            writer.Uint64(m_core->getBlockDifficulty(height));

            writer.Key("reward");
            writer.Uint64(reward);

            writer.Key("num_txes");
            writer.Uint64(extraDetails.transactions.size());

            writer.Key("block_size");
            writer.Uint64(extraDetails.blockSize);
        }
        writer.EndObject();
    }
    writer.EndObject();

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getBlockHeaderByHeight(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    const auto params = getObjectFromJSON(body, "params");
    const auto height = getUint64FromJSON(params, "height");
    const auto topHeight = m_core->getTopBlockIndex();

    if (height > topHeight)
    {
        failJsonRpcRequest(
            -2,
            "Requested block header for a height that is higher than the current "
            "blockchain height! Current height: " + std::to_string(topHeight),
            res
        );

        return {SUCCESS, 200};
    }

    const auto hash = m_core->getBlockHashByIndex(height);
    const auto block = m_core->getBlockByHash(hash);

    const auto outputs = block.baseTransaction.outputs;
    const auto extraDetails = m_core->getBlockDetails(hash);

    const uint64_t reward = std::accumulate(outputs.begin(), outputs.end(), 0ull,
        [](const auto acc, const auto out) {
            return acc + out.amount;
        }
    );

    writer.StartObject();

    writer.Key("jsonrpc");
    writer.String("2.0");

    writer.Key("result");
    writer.StartObject();
    {
        writer.Key("status");
        writer.String("OK");

        writer.Key("block_header");
        writer.StartObject();
        {
            writer.Key("major_version");
            writer.Uint64(block.majorVersion);

            writer.Key("minor_version");
            writer.Uint64(block.minorVersion);

            writer.Key("timestamp");
            writer.Uint64(block.timestamp);

            writer.Key("prev_hash");
            writer.String(Common::podToHex(block.previousBlockHash));

            writer.Key("nonce");
            writer.Uint64(block.nonce);

            writer.Key("orphan_status");
            writer.Bool(extraDetails.isAlternative);

            writer.Key("height");
            writer.Uint64(height);

            writer.Key("depth");
            writer.Uint64(topHeight - height);

            writer.Key("hash");
            writer.String(Common::podToHex(hash));

            writer.Key("difficulty");
            writer.Uint64(m_core->getBlockDifficulty(height));

            writer.Key("reward");
            writer.Uint64(reward);

            writer.Key("num_txes");
            writer.Uint64(extraDetails.transactions.size());

            writer.Key("block_size");
            writer.Uint64(extraDetails.blockSize);
        }
        writer.EndObject();
    }
    writer.EndObject();

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getBlocksByHeight(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    const auto params = getObjectFromJSON(body, "params");
    const auto height = getUint64FromJSON(params, "height");
    const auto topHeight = m_core->getTopBlockIndex();

    if (height > topHeight)
    {
        failJsonRpcRequest(
            -2,
            "Requested block header for a height that is higher than the current "
            "blockchain height! Current height: " + std::to_string(topHeight),
            res
        );

        return {SUCCESS, 200};
    }

    writer.StartObject();

    writer.Key("jsonrpc");
    writer.String("2.0");

    writer.Key("result");
    writer.StartObject();
    {
        writer.Key("status");
        writer.String("OK");

        const uint64_t MAX_BLOCKS_COUNT = 30;
        const uint64_t startHeight = height < MAX_BLOCKS_COUNT ? 0 : height - MAX_BLOCKS_COUNT;

        writer.Key("blocks");
        writer.StartArray();
        {
            for (uint64_t i = height; i >= startHeight; i--)
            {
                writer.StartObject();

                const auto hash = m_core->getBlockHashByIndex(i);
                const auto block = m_core->getBlockByHash(hash);
                const auto extraDetails = m_core->getBlockDetails(hash);

                writer.Key("cumul_size");
                writer.Uint64(extraDetails.blockSize);

                writer.Key("difficulty");
                writer.Uint64(extraDetails.difficulty);

                writer.Key("hash");
                writer.String(Common::podToHex(hash));

                writer.Key("height");
                writer.Uint64(i);

                writer.Key("timestamp");
                writer.Uint64(block.timestamp);

                /* Plus one for coinbase tx */
                writer.Key("tx_count");
                writer.Uint64(block.transactionHashes.size() + 1);

                writer.EndObject();
            }
        }
        writer.EndArray();
    }
    writer.EndObject();

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getBlockDetailsByHash(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    const auto params = getObjectFromJSON(body, "params");
    const auto hashStr = getStringFromJSON(params, "hash");
    const auto topHeight = m_core->getTopBlockIndex();

    Crypto::Hash hash;

    if (hashStr.length() == 64)
    {
        if (!Common::podFromHex(hashStr, hash))
        {
            failJsonRpcRequest(
                -1,
                "Block hash specified is not a valid hex!",
                res
            );

            return {SUCCESS, 200};
        }
    }
    else
    {
        /* Hash parameter can be both a hash string, and a number... because cryptonote.. */
        try
        {
            uint64_t height = std::stoull(hashStr);

            hash = m_core->getBlockHashByIndex(height - 1);

            if (hash == Constants::NULL_HASH)
            {
                failJsonRpcRequest(
                    -2,
                    "Requested hash for a height that is higher than the current "
                    "blockchain height! Current height: " + std::to_string(topHeight),
                    res
                );

                return {SUCCESS, 200};
            }
        }
        catch (const std::out_of_range &)
        {
            failJsonRpcRequest(
                -1,
                "Block hash specified is not valid!",
                res
            );

            return {SUCCESS, 200};
        }
        catch (const std::invalid_argument &)
        {
            failJsonRpcRequest(
                -1,
                "Block hash specified is not valid!",
                res
            );

            return {SUCCESS, 200};
        }
    }

    const auto block = m_core->getBlockByHash(hash);
    const auto extraDetails = m_core->getBlockDetails(hash);
    const auto height = CryptoNote::CachedBlock(block).getBlockIndex();
    const auto outputs = block.baseTransaction.outputs;

    const uint64_t reward = std::accumulate(outputs.begin(), outputs.end(), 0ull,
        [](const auto acc, const auto out) {
            return acc + out.amount;
        }
    );

    const uint64_t blockSizeMedian = std::max(
        extraDetails.sizeMedian,
        static_cast<uint64_t>(
            m_core->getCurrency().blockGrantedFullRewardZoneByBlockVersion(block.majorVersion)
        )
    );

    std::vector<Crypto::Hash> ignore;
    std::vector<std::vector<uint8_t>> transactions;

    m_core->getTransactions(block.transactionHashes, transactions, ignore);

    writer.StartObject();

    writer.Key("jsonrpc");
    writer.String("2.0");

    writer.Key("result");
    writer.StartObject();
    {
        writer.Key("status");
        writer.String("OK");

        writer.Key("block");
        writer.StartObject();
        {
            writer.Key("major_version");
            writer.Uint64(block.majorVersion);

            writer.Key("minor_version");
            writer.Uint64(block.minorVersion);

            writer.Key("timestamp");
            writer.Uint64(block.timestamp);

            writer.Key("prev_hash");
            writer.String(Common::podToHex(block.previousBlockHash));

            writer.Key("nonce");
            writer.Uint64(block.nonce);

            writer.Key("orphan_status");
            writer.Bool(extraDetails.isAlternative);

            writer.Key("height");
            writer.Uint64(height);

            writer.Key("depth");
            writer.Uint64(topHeight - height);

            writer.Key("hash");
            writer.String(Common::podToHex(hash));

            writer.Key("difficulty");
            writer.Uint64(m_core->getBlockDifficulty(height));

            writer.Key("reward");
            writer.Uint64(reward);

            writer.Key("blockSize");
            writer.Uint64(extraDetails.blockSize);

            writer.Key("transactionsCumulativeSize");
            writer.Uint64(extraDetails.transactionsCumulativeSize);

            writer.Key("alreadyGeneratedCoins");
            writer.String(std::to_string(extraDetails.alreadyGeneratedCoins));

            writer.Key("alreadyGeneratedTransactions");
            writer.Uint64(extraDetails.alreadyGeneratedTransactions);

            writer.Key("sizeMedian");
            writer.Uint64(extraDetails.sizeMedian);

            writer.Key("baseReward");
            writer.Uint64(extraDetails.baseReward);

            writer.Key("penalty");
            writer.Double(extraDetails.penalty);

            writer.Key("effectiveSizeMedian");
            writer.Uint64(blockSizeMedian);

            uint64_t totalFee = 0;

            writer.Key("transactions");
            writer.StartArray();
            {
                /* Coinbase transaction */
                writer.StartObject();
                {
                    const auto txOutputs = block.baseTransaction.outputs;

                    const uint64_t outputAmount = std::accumulate(txOutputs.begin(), txOutputs.end(), 0ull,
                        [](const auto acc, const auto out) {
                            return acc + out.amount;
                        }
                    );

                    writer.Key("hash");
                    writer.String(Common::podToHex(getObjectHash(block.baseTransaction)));

                    writer.Key("fee");
                    writer.Uint64(0);

                    writer.Key("amount_out");
                    writer.Uint64(outputAmount);

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

                        const uint64_t outputAmount = std::accumulate(tx.outputs.begin(), tx.outputs.end(), 0ull,
                            [](const auto acc, const auto out) {
                                return acc + out.amount;
                            }
                        );

                        const uint64_t inputAmount = std::accumulate(tx.inputs.begin(), tx.inputs.end(), 0ull,
                            [](const auto acc, const auto in) {
                                if (in.type() == typeid(CryptoNote::KeyInput))
                                {
                                    return acc + boost::get<CryptoNote::KeyInput>(in).amount;
                                }

                                return acc;
                            }
                        );

                        const uint64_t fee = inputAmount - outputAmount;

                        writer.Key("hash");
                        writer.String(Common::podToHex(getObjectHash(tx)));

                        writer.Key("fee");
                        writer.Uint64(fee);

                        writer.Key("amount_out");
                        writer.Uint64(outputAmount);

                        writer.Key("size");
                        writer.Uint64(getObjectBinarySize(tx));

                        totalFee += fee;
                    }
                    writer.EndObject();
                }
            }
            writer.EndArray();

            writer.Key("totalFeeAmount");
            writer.Uint64(totalFee);
        }
        writer.EndObject();
    }
    writer.EndObject();

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getTransactionDetailsByHash(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    const auto params = getObjectFromJSON(body, "params");
    const auto hashStr = getStringFromJSON(params, "hash");

    Crypto::Hash hash;

    if (!Common::podFromHex(hashStr, hash))
    {
        failJsonRpcRequest(
            -1,
            "Block hash specified is not a valid hex!",
            res
        );

        return {SUCCESS, 200};
    }

    std::vector<Crypto::Hash> ignore;
    std::vector<std::vector<uint8_t>> rawTXs;
    std::vector<Crypto::Hash> hashes { hash };

    m_core->getTransactions(hashes, rawTXs, ignore);

    if (rawTXs.size() != 1)
    {
        failJsonRpcRequest(
            -1,
            "Block hash specified does not exist!",
            res
        );

        return {SUCCESS, 200};
    }

    CryptoNote::Transaction transaction;
    CryptoNote::TransactionDetails txDetails = m_core->getTransactionDetails(hash);

    const uint64_t blockHeight = txDetails.blockIndex;
    const auto blockHash = m_core->getBlockHashByIndex(blockHeight);
    const auto block = m_core->getBlockByHash(blockHash);
    const auto extraDetails = m_core->getBlockDetails(blockHash);

    fromBinaryArray(transaction, rawTXs[0]);

    writer.StartObject();

    writer.Key("jsonrpc");
    writer.String("2.0");

    writer.Key("result");
    writer.StartObject();
    {
        writer.Key("status");
        writer.String("OK");

        writer.Key("block");
        writer.StartObject();
        {
            writer.Key("cumul_size");
            writer.Uint64(extraDetails.blockSize);

            writer.Key("difficulty");
            writer.Uint64(extraDetails.difficulty);

            writer.Key("hash");
            writer.String(Common::podToHex(blockHash));

            writer.Key("height");
            writer.Uint64(blockHeight);

            writer.Key("timestamp");
            writer.Uint64(block.timestamp);

            /* Plus one for coinbase tx */
            writer.Key("tx_count");
            writer.Uint64(block.transactionHashes.size() + 1);
        }
        writer.EndObject();

        writer.Key("tx");
        writer.StartObject();
        {
            writer.Key("extra");
            writer.String(Common::podToHex(transaction.extra));

            writer.Key("unlock_time");
            writer.Uint64(transaction.unlockTime);

            writer.Key("version");
            writer.Uint64(transaction.version);

            writer.Key("vin");
            writer.StartArray();
            {
                for (const auto input : transaction.inputs)
                {
                    const auto type = input.type() == typeid(CryptoNote::BaseInput)
                        ? "ff"
                        : "02";

                    writer.StartObject();
                    {
                        writer.Key("type");
                        writer.String(type);

                        writer.Key("value");
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

                                writer.Key("k_image");
                                writer.String(Common::podToHex(keyInput.keyImage));

                                writer.Key("amount");
                                writer.Uint64(keyInput.amount);

                                writer.Key("key_offsets");
                                writer.StartArray();
                                {
                                    for (const auto index : keyInput.outputIndexes)
                                    {
                                        writer.Uint(index);
                                    }
                                }
                                writer.EndArray();
                            }
                        }
                        writer.EndObject();
                    }
                    writer.EndObject();
                }
            }
            writer.EndArray();

            writer.Key("vout");
            writer.StartArray();
            {
                for (const auto output : transaction.outputs)
                {
                    writer.StartObject();
                    {
                        writer.Key("amount");
                        writer.Uint64(output.amount);

                        writer.Key("target");
                        writer.StartObject();
                        {
                            writer.Key("data");
                            writer.StartObject();
                            {
                                writer.Key("key");
                                writer.String(Common::podToHex(boost::get<CryptoNote::KeyOutput>(output.target).key));
                            }
                            writer.EndObject();

                            writer.Key("type");
                            writer.String("02");
                        }
                        writer.EndObject();
                    }
                    writer.EndObject();
                }
            }
            writer.EndArray();
        }
        writer.EndObject();

        writer.Key("txDetails");
        writer.StartObject();
        {
            writer.Key("hash");
            writer.String(Common::podToHex(txDetails.hash));

            writer.Key("amount_out");
            writer.Uint64(txDetails.totalOutputsAmount);

            writer.Key("fee");
            writer.Uint64(txDetails.fee);

            writer.Key("mixin");
            writer.Uint64(txDetails.mixin);

            writer.Key("paymentId");
            if (txDetails.paymentId == Constants::NULL_HASH)
            {
                writer.String("");
            }
            else
            {
                writer.String(Common::podToHex(txDetails.paymentId));
            }

            writer.Key("size");
            writer.Uint64(txDetails.size);
        }
        writer.EndObject();
    }
    writer.EndObject();

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

    writer.StartObject();

    writer.Key("jsonrpc");
    writer.String("2.0");

    writer.Key("result");
    writer.StartObject();
    {
        writer.Key("status");
        writer.String("OK");

        writer.Key("transactions");
        writer.StartArray();
        {
            for (const auto tx : m_core->getPoolTransactions())
            {
                writer.StartObject();

                const uint64_t outputAmount = std::accumulate(tx.outputs.begin(), tx.outputs.end(), 0ull,
                    [](const auto acc, const auto out) {
                        return acc + out.amount;
                    }
                );

                const uint64_t inputAmount = std::accumulate(tx.inputs.begin(), tx.inputs.end(), 0ull,
                    [](const auto acc, const auto in) {
                        if (in.type() == typeid(CryptoNote::KeyInput))
                        {
                            return acc + boost::get<CryptoNote::KeyInput>(in).amount;
                        }

                        return acc;
                    }
                );

                const uint64_t fee = inputAmount - outputAmount;

                writer.Key("hash");
                writer.String(Common::podToHex(getObjectHash(tx)));

                writer.Key("fee");
                writer.Uint64(fee);

                writer.Key("amount_out");
                writer.Uint64(outputAmount);

                writer.Key("size");
                writer.Uint64(getObjectBinarySize(tx));

                writer.EndObject();
            }
        }
        writer.EndArray();
    }
    writer.EndObject();

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::queryBlocksLite(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    uint64_t timestamp = 0;

    if (hasMember(body, "timestamp"))
    {
        timestamp = getUint64FromJSON(body, "timestamp");
    }

    std::vector<Crypto::Hash> knownBlockHashes;

    if (hasMember(body, "blockIds"))
    {
        for (const auto &hashStrJson : getArrayFromJSON(body, "blockIds"))
        {
            Crypto::Hash hash;

            if (!Common::podFromHex(getStringFromJSONString(hashStrJson), hash))
            {
                failRequest(400, "Block hash specified is not a valid hex string!", res);
                return {SUCCESS, 400};
            }

            knownBlockHashes.push_back(hash);
        }
    }

    uint32_t startHeight;
    uint32_t currentHeight;
    uint32_t fullOffset;

    std::vector<CryptoNote::BlockShortInfo> blocks;

    if (!m_core->queryBlocksLite(knownBlockHashes, timestamp, startHeight, currentHeight, fullOffset, blocks))
    {
        failRequest(500, "Internal error: failed to queryblockslite", res);
        return {SUCCESS, 500};
    }

    writer.StartObject();

    writer.Key("fullOffset");
    writer.Uint64(fullOffset);

    writer.Key("currentHeight");
    writer.Uint64(currentHeight);

    writer.Key("startHeight");
    writer.Uint64(startHeight);

    writer.Key("items");
    writer.StartArray();
    {
        for (const auto block : blocks)
        {
            writer.StartObject();
            {
                writer.Key("blockShortInfo.block");
                writer.StartArray();
                {
                    for (const auto c : block.block)
                    {
                        writer.Uint64(c);
                    }
                }
                writer.EndArray();

                writer.Key("blockShortInfo.blockId");
                writer.String(Common::podToHex(block.blockId));

                writer.Key("blockShortInfo.txPrefixes");
                writer.StartArray();
                {
                    for (const auto prefix : block.txPrefixes)
                    {
                        writer.StartObject();
                        {
                            writer.Key("transactionPrefixInfo.txHash");
                            writer.String(Common::podToHex(prefix.txHash));

                            writer.Key("transactionPrefixInfo.txPrefix");
                            writer.StartObject();
                            {
                                writer.Key("extra");
                                writer.String(Common::toHex(prefix.txPrefix.extra));

                                writer.Key("unlock_time");
                                writer.Uint64(prefix.txPrefix.unlockTime);

                                writer.Key("version");
                                writer.Uint64(prefix.txPrefix.version);

                                writer.Key("vin");
                                writer.StartArray();
                                {
                                    for (const auto input : prefix.txPrefix.inputs)
                                    {
                                        const auto type = input.type() == typeid(CryptoNote::BaseInput)
                                            ? "ff"
                                            : "02";

                                        writer.StartObject();
                                        {
                                            writer.Key("type");
                                            writer.String(type);

                                            writer.Key("value");
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

                                                    writer.Key("k_image");
                                                    writer.String(Common::podToHex(keyInput.keyImage));

                                                    writer.Key("amount");
                                                    writer.Uint64(keyInput.amount);

                                                    writer.Key("key_offsets");
                                                    writer.StartArray();
                                                    {
                                                        for (const auto index : keyInput.outputIndexes)
                                                        {
                                                            writer.Uint(index);
                                                        }
                                                    }
                                                    writer.EndArray();
                                                }
                                            }
                                            writer.EndObject();
                                        }
                                        writer.EndObject();
                                    }
                                }
                                writer.EndArray();

                                writer.Key("vout");
                                writer.StartArray();
                                {
                                    for (const auto output : prefix.txPrefix.outputs)
                                    {
                                        writer.StartObject();
                                        {
                                            writer.Key("amount");
                                            writer.Uint64(output.amount);

                                            writer.Key("target");
                                            writer.StartObject();
                                            {
                                                writer.Key("data");
                                                writer.StartObject();
                                                {
                                                    writer.Key("key");
                                                    writer.String(Common::podToHex(boost::get<CryptoNote::KeyOutput>(output.target).key));
                                                }
                                                writer.EndObject();

                                                writer.Key("type");
                                                writer.String("02");
                                            }
                                            writer.EndObject();
                                        }
                                        writer.EndObject();
                                    }
                                }
                                writer.EndArray();
                            }
                            writer.EndObject();
                        }
                        writer.EndObject();
                    }
                }
                writer.EndArray();
            }
            writer.EndObject();
        }
    }
    writer.EndArray();

    writer.Key("status");
    writer.String("OK");

    writer.EndObject();

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

    for (const auto &hashStr : getArrayFromJSON(body, "transactionHashes"))
    {
        Crypto::Hash hash;

        if (!Common::podFromHex(getStringFromJSONString(hashStr), hash))
        {
            failRequest(400, "Transaction hash specified is not a valid hex string!", res);
            return {SUCCESS, 400};
        }

        transactionHashes.insert(hash);
    }

    std::unordered_set<Crypto::Hash> transactionsInPool;
    std::unordered_set<Crypto::Hash> transactionsInBlock;
    std::unordered_set<Crypto::Hash> transactionsUnknown;

    const bool success = m_core->getTransactionsStatus(
        transactionHashes, transactionsInPool, transactionsInBlock, transactionsUnknown
    );

    if (!success)
    {
        failRequest(500, "Internal error: failed to getTransactionsStatus", res);
        return {SUCCESS, 500};
    }

    writer.StartObject();

    writer.Key("transactionsInBlock");
    writer.StartArray();
    {
        for (const auto &hash : transactionsInBlock)
        {
            writer.String(Common::podToHex(hash));
        }
    }
    writer.EndArray();

    writer.Key("transactionsInPool");
    writer.StartArray();
    {
        for (const auto &hash : transactionsInPool)
        {
            writer.String(Common::podToHex(hash));
        }
    }
    writer.EndArray();

    writer.Key("transactionsUnknown");
    writer.StartArray();
    {
        for (const auto &hash : transactionsUnknown)
        {
            writer.String(Common::podToHex(hash));
        }
    }
    writer.EndArray();

    writer.Key("status");
    writer.String("OK");

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getPoolChanges(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    Crypto::Hash lastBlockHash;

    if (!Common::podFromHex(getStringFromJSON(body, "tailBlockId"), lastBlockHash))
    {
        failRequest(400, "tailBlockId specified is not a valid hex string!", res);
        return {SUCCESS, 400};
    }

    std::vector<Crypto::Hash> knownHashes;

    for (const auto &hashStr : getArrayFromJSON(body, "knownTxsIds"))
    {
        Crypto::Hash hash;

        if (!Common::podFromHex(getStringFromJSONString(hashStr), hash))
        {
            failRequest(400, "Transaction hash specified is not a valid hex string!", res);
            return {SUCCESS, 400};
        }

        knownHashes.push_back(hash);
    }

    std::vector<CryptoNote::TransactionPrefixInfo> addedTransactions;
    std::vector<Crypto::Hash> deletedTransactions;

    const bool atTopOfChain = m_core->getPoolChangesLite(
        lastBlockHash, knownHashes, addedTransactions, deletedTransactions
    );

    writer.StartObject();

    writer.Key("addedTxs");
    writer.StartArray();
    {
        for (const auto prefix: addedTransactions)
        {
            writer.StartObject();
            {
                writer.Key("transactionPrefixInfo.txHash");
                writer.String(Common::podToHex(prefix.txHash));

                writer.Key("transactionPrefixInfo.txPrefix");
                writer.StartObject();
                {
                    writer.Key("extra");
                    writer.String(Common::toHex(prefix.txPrefix.extra));

                    writer.Key("unlock_time");
                    writer.Uint64(prefix.txPrefix.unlockTime);

                    writer.Key("version");
                    writer.Uint64(prefix.txPrefix.version);

                    writer.Key("vin");
                    writer.StartArray();
                    {
                        for (const auto input : prefix.txPrefix.inputs)
                        {
                            const auto type = input.type() == typeid(CryptoNote::BaseInput)
                                ? "ff"
                                : "02";

                            writer.StartObject();
                            {
                                writer.Key("type");
                                writer.String(type);

                                writer.Key("value");
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

                                        writer.Key("k_image");
                                        writer.String(Common::podToHex(keyInput.keyImage));

                                        writer.Key("amount");
                                        writer.Uint64(keyInput.amount);

                                        writer.Key("key_offsets");
                                        writer.StartArray();
                                        {
                                            for (const auto index : keyInput.outputIndexes)
                                            {
                                                writer.Uint(index);
                                            }
                                        }
                                        writer.EndArray();
                                    }
                                }
                                writer.EndObject();
                            }
                            writer.EndObject();
                        }
                    }
                    writer.EndArray();

                    writer.Key("vout");
                    writer.StartArray();
                    {
                        for (const auto output : prefix.txPrefix.outputs)
                        {
                            writer.StartObject();
                            {
                                writer.Key("amount");
                                writer.Uint64(output.amount);

                                writer.Key("target");
                                writer.StartObject();
                                {
                                    writer.Key("data");
                                    writer.StartObject();
                                    {
                                        writer.Key("key");
                                        writer.String(Common::podToHex(boost::get<CryptoNote::KeyOutput>(output.target).key));
                                    }
                                    writer.EndObject();

                                    writer.Key("type");
                                    writer.String("02");
                                }
                                writer.EndObject();
                            }
                            writer.EndObject();
                        }
                    }
                    writer.EndArray();
                }
                writer.EndObject();
            }
            writer.EndObject();
        }
    }
    writer.EndArray();

    writer.Key("deletedTxsIds");
    writer.StartArray();
    {
        for (const auto hash : deletedTransactions)
        {
            writer.String(Common::podToHex(hash));
        }
    }
    writer.EndArray();

    writer.Key("isTailBlockActual");
    writer.Bool(atTopOfChain);

    writer.Key("status");
    writer.String("OK");

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::queryBlocksDetailed(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    uint64_t timestamp = 0;

    if (hasMember(body, "timestamp"))
    {
        timestamp = getUint64FromJSON(body, "timestamp");
    }

    std::vector<Crypto::Hash> knownBlockHashes;

    if (hasMember(body, "blockIds"))
    {
        for (const auto &hashStrJson : getArrayFromJSON(body, "blockIds"))
        {
            Crypto::Hash hash;

            if (!Common::podFromHex(getStringFromJSONString(hashStrJson), hash))
            {
                failRequest(400, "Block hash specified is not a valid hex string!", res);
                return {SUCCESS, 400};
            }

            knownBlockHashes.push_back(hash);
        }
    }

    uint64_t startHeight;
    uint64_t currentHeight;
    uint64_t fullOffset;

    uint64_t blockCount = CryptoNote::BLOCKS_SYNCHRONIZING_DEFAULT_COUNT;

    if (hasMember(body, "blockCount"))
    {
        blockCount = getUint64FromJSON(body, "blockCount");
    }

    std::vector<CryptoNote::BlockDetails> blocks;

    if (!m_core->queryBlocksDetailed(knownBlockHashes, timestamp, startHeight, currentHeight, fullOffset, blocks, blockCount))
    {
        failRequest(500, "Internal error: failed to queryblockslite", res);
        return {SUCCESS, 500};
    }

    writer.StartObject();

    writer.Key("fullOffset");
    writer.Uint64(fullOffset);

    writer.Key("currentHeight");
    writer.Uint64(currentHeight);

    writer.Key("startHeight");
    writer.Uint64(startHeight);

    writer.Key("blocks");
    writer.StartArray();
    {
        for (const auto block : blocks)
        {
            writer.StartObject();
            {
                writer.Key("major_version");
                writer.Uint64(block.majorVersion);

                writer.Key("minor_version");
                writer.Uint64(block.minorVersion);

                writer.Key("timestamp");
                writer.Uint64(block.timestamp);

                writer.Key("prevBlockHash");
                writer.String(Common::podToHex(block.prevBlockHash));

                writer.Key("index");
                writer.Uint64(block.index);

                writer.Key("hash");
                writer.String(Common::podToHex(block.hash));

                writer.Key("difficulty");
                writer.Uint64(block.difficulty);

                writer.Key("reward");
                writer.Uint64(block.reward);

                writer.Key("blockSize");
                writer.Uint64(block.blockSize);

                writer.Key("alreadyGeneratedCoins");
                writer.String(std::to_string(block.alreadyGeneratedCoins));

                writer.Key("alreadyGeneratedTransactions");
                writer.Uint64(block.alreadyGeneratedTransactions);

                writer.Key("sizeMedian");
                writer.Uint64(block.sizeMedian);

                writer.Key("baseReward");
                writer.Uint64(block.baseReward);

                writer.Key("nonce");
                writer.Uint64(block.nonce);

                writer.Key("totalFeeAmount");
                writer.Uint64(block.totalFeeAmount);

                writer.Key("transactionsCumulativeSize");
                writer.Uint64(block.transactionsCumulativeSize);

                writer.Key("transactions");
                writer.StartArray();
                {
                    for (const auto &tx : block.transactions)
                    {
                        writer.StartObject();
                        {
                            writer.Key("blockHash");
                            writer.String(Common::podToHex(block.hash));

                            writer.Key("blockIndex");
                            writer.Uint64(block.index);

                            writer.Key("extra");
                            writer.StartObject();
                            {
                                writer.Key("nonce");
                                writer.StartArray();
                                {
                                    for (const auto c : tx.extra.nonce)
                                    {
                                        writer.Uint64(c);
                                    }
                                }
                                writer.EndArray();

                                writer.Key("publicKey");
                                writer.String(Common::podToHex(tx.extra.publicKey));

                                writer.Key("raw");
                                writer.String(Common::toHex(tx.extra.raw));
                            }
                            writer.EndObject();

                            writer.Key("fee");
                            writer.Uint64(tx.fee);

                            writer.Key("hash");
                            writer.String(Common::podToHex(tx.hash));

                            writer.Key("inBlockchain");
                            writer.Bool(tx.inBlockchain);

                            writer.Key("inputs");
                            writer.StartArray();
                            {
                                for (const auto &input : tx.inputs)
                                {
                                    const auto type = input.type() == typeid(CryptoNote::BaseInputDetails)
                                        ? "ff"
                                        : "02";

                                    writer.StartObject();
                                    {
                                        writer.Key("type");
                                        writer.String(type);

                                        writer.Key("data");
                                        writer.StartObject();
                                        {
                                            if (input.type() == typeid(CryptoNote::BaseInputDetails))
                                            {
                                                const auto in = boost::get<CryptoNote::BaseInputDetails>(input);

                                                writer.Key("amount");
                                                writer.Uint64(in.amount);

                                                writer.Key("input");
                                                writer.StartObject();
                                                {
                                                    writer.Key("height");
                                                    writer.Uint64(in.input.blockIndex);
                                                }
                                                writer.EndObject();
                                            }
                                            else
                                            {
                                                const auto in = boost::get<CryptoNote::KeyInputDetails>(input);

                                                writer.Key("input");
                                                writer.StartObject();
                                                {
                                                    writer.Key("amount");
                                                    writer.Uint64(in.input.amount);

                                                    writer.Key("k_image");
                                                    writer.String(Common::podToHex(in.input.keyImage));

                                                    writer.Key("key_offsets");
                                                    writer.StartArray();
                                                    {
                                                        for (const auto index : in.input.outputIndexes)
                                                        {
                                                            writer.Uint(index);
                                                        }
                                                    }
                                                    writer.EndArray();

                                                }
                                                writer.EndObject();

                                                writer.Key("mixin");
                                                writer.Uint64(in.mixin);

                                                writer.Key("output");
                                                writer.StartObject();
                                                {
                                                    writer.Key("transactionHash");
                                                    writer.String(Common::podToHex(in.output.transactionHash));

                                                    writer.Key("number");
                                                    writer.Uint64(in.output.number);
                                                }
                                                writer.EndObject();
                                            }
                                        }
                                        writer.EndObject();
                                    }
                                    writer.EndObject();
                                }
                            }
                            writer.EndArray();

                            writer.Key("mixin");
                            writer.Uint64(tx.mixin);

                            writer.Key("outputs");
                            writer.StartArray();
                            {
                                for (const auto &output : tx.outputs)
                                {
                                    writer.StartObject();
                                    {
                                        writer.Key("globalIndex");
                                        writer.Uint64(output.globalIndex);

                                        writer.Key("output");
                                        writer.StartObject();
                                        {
                                            writer.Key("amount");
                                            writer.Uint64(output.output.amount);

                                            writer.Key("target");
                                            writer.StartObject();
                                            {
                                                writer.Key("data");
                                                writer.StartObject();
                                                {
                                                    writer.Key("key");
                                                    writer.String(Common::podToHex(boost::get<CryptoNote::KeyOutput>(output.output.target).key));
                                                }
                                                writer.EndObject();

                                                writer.Key("type");
                                                writer.String("02");
                                            }
                                            writer.EndObject();
                                        }
                                        writer.EndObject();
                                    }
                                    writer.EndObject();
                                }
                            }
                            writer.EndArray();

                            writer.Key("paymentId");
                            writer.String(Common::podToHex(tx.paymentId));

                            writer.Key("signatures");
                            writer.StartArray();
                            {
                                int i = 0;

                                for (const auto &sigs : tx.signatures)
                                {
                                    for (const auto &sig : sigs)
                                    {
                                        writer.StartObject();
                                        {
                                            writer.Key("first");
                                            writer.Uint64(i);

                                            writer.Key("second");
                                            writer.String(Common::podToHex(sig));
                                        }
                                        writer.EndObject();
                                    }

                                    i++;
                                }
                            }
                            writer.EndArray();

                            writer.Key("signaturesSize");
                            writer.Uint64(tx.signatures.size());

                            writer.Key("size");
                            writer.Uint64(tx.size);

                            writer.Key("timestamp");
                            writer.Uint64(tx.timestamp);

                            writer.Key("totalInputsAmount");
                            writer.Uint64(tx.totalInputsAmount);

                            writer.Key("totalOutputsAmount");
                            writer.Uint64(tx.totalOutputsAmount);

                            writer.Key("unlockTime");
                            writer.Uint64(tx.unlockTime);
                        }
                        writer.EndObject();
                    }
                }
                writer.EndArray();
            }
            writer.EndObject();
        }
    }
    writer.EndArray();

    writer.Key("status");
    writer.String("OK");

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

/* Deprecated. Use getGlobalIndexes instead. */
std::tuple<Error, uint16_t> RpcServer::getGlobalIndexesDeprecated(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    Crypto::Hash hash;

    if (!Common::podFromHex(getStringFromJSON(body, "txid"), hash))
    {
        failRequest(400, "txid specified is not a valid hex string!", res);
        return {SUCCESS, 400};
    }

    std::vector<uint32_t> indexes;

    const bool success = m_core->getTransactionGlobalIndexes(hash, indexes);

    if (!success)
    {
        failRequest(500, "Internal error: Failed to getTransactionGlobalIndexes", res);
        return {SUCCESS, 500};
    }

    writer.StartObject();

    writer.Key("o_indexes");

    writer.StartArray();
    {
        for (const auto &index : indexes)
        {
            writer.Uint64(index);
        }
    }
    writer.EndArray();

    writer.Key("status");
    writer.String("OK");

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

std::tuple<Error, uint16_t> RpcServer::getRawBlocks(
    const httplib::Request &req,
    httplib::Response &res,
    const rapidjson::Document &body)
{
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();

    std::vector<Crypto::Hash> blockHashCheckpoints;

    if (hasMember(body, "blockHashCheckpoints"))
    {
        for (const auto &jsonHash : getArrayFromJSON(body, "blockHashCheckpoints"))
        {
            std::string hashStr = jsonHash.GetString();

            Crypto::Hash hash;
            Common::podFromHex(hashStr, hash);

            blockHashCheckpoints.push_back(hash);
        }
    }

    const uint64_t startHeight = hasMember(body, "startHeight")
        ? getUint64FromJSON(body, "startHeight")
        : 0;

    const uint64_t startTimestamp = hasMember(body, "startTimestamp")
        ? getUint64FromJSON(body, "startTimestamp")
        : 0;

    const uint64_t blockCount = hasMember(body, "blockCount")
        ? getUint64FromJSON(body, "blockCount")
        : 100;

    const bool skipCoinbaseTransactions = hasMember(body, "skipCoinbaseTransactions")
        ? getBoolFromJSON(body, "skipCoinbaseTransactions")
        : false;

    std::vector<CryptoNote::RawBlock> blocks;
    std::optional<WalletTypes::TopBlock> topBlockInfo;

    const bool success = m_core->getRawBlocks(
        blockHashCheckpoints,
        startHeight,
        startTimestamp,
        blockCount,
        skipCoinbaseTransactions,
        blocks,
        topBlockInfo
    );

    if (!success)
    {
        return {SUCCESS, 500};
    }

    writer.Key("items");
    writer.StartArray();
    {
        for (const auto &block : blocks)
        {
            writer.StartObject();

            writer.Key("block");
            writer.String(Common::toHex(block.block));

            writer.Key("transactions");
            writer.StartArray();
            for (const auto &transaction : block.transactions)
            {
                writer.String(Common::toHex(transaction));
            }
            writer.EndArray();

            writer.EndObject();
        }
    }
    writer.EndArray();

    if (topBlockInfo)
    {
        writer.Key("topBlock");
        writer.StartObject();
        {
            writer.Key("hash");
            writer.String(Common::podToHex(topBlockInfo->hash));

            writer.Key("height");
            writer.Uint64(topBlockInfo->height);
        }
        writer.EndObject();
    }

    writer.Key("synced");
    writer.Bool(blocks.empty());

    writer.Key("status");
    writer.String("OK");

    writer.EndObject();

    res.body = sb.GetString();

    return {SUCCESS, 200};
}

