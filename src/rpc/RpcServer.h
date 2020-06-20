// Copyright (c) 2019-2020, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "JsonHelper.h"
#include "httplib.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include <cryptonotecore/Core.h>
#include <cryptonoteprotocol/CryptoNoteProtocolHandlerCommon.h>
#include <errors/Errors.h>
#include <future>
#include <memory>
#include <optional>
#include <p2p/NetNode.h>
#include <string>

enum class RpcMode
{
    Default = 0,
    BlockExplorerEnabled = 1,
    AllMethodsEnabled = 2,
};

class RpcServer
{
  public:
    ////////////////////////////////
    /* Constructors / Destructors */
    ////////////////////////////////
    RpcServer(
        const uint16_t bindPort,
        const std::string rpcBindIp,
        const std::string corsHeader,
        const std::string feeAddress,
        const uint64_t feeAmount,
        const RpcMode rpcMode,
        const std::shared_ptr<CryptoNote::Core> core,
        const std::shared_ptr<CryptoNote::NodeServer> p2p,
        const std::shared_ptr<CryptoNote::ICryptoNoteProtocolHandler> syncManager);

    ~RpcServer();

    /////////////////////////////
    /* Public member functions */
    /////////////////////////////

    /* Starts the server. */
    void start();

    /* Stops the server. */
    void stop();

    /* Gets the IP/port combo the server is running on */
    std::tuple<std::string, uint16_t> getConnectionInfo();

  private:
    //////////////////////////////
    /* Private member functions */
    //////////////////////////////

    /* Starts listening for requests on the server */
    void listen();

    std::optional<rapidjson::Document>
        getJsonBody(const httplib::Request &req, httplib::Response &res, const bool bodyRequired);

    /* Handles stuff like parsing json and then forwards onto the handler */
    void middleware(
        const httplib::Request &req,
        httplib::Response &res,
        const RpcMode routePermissions,
        const bool bodyRequired,
        const bool syncRequired,
        std::function<std::tuple<Error, uint16_t>(
            const httplib::Request &req,
            httplib::Response &res,
            const rapidjson::Document &body)> handler);

    void failRequest(const Error error, httplib::Response &res);

    uint64_t calculateTotalFeeAmount(const std::vector<Crypto::Hash> &transactionHashes);

    void generateBlockHeader(
        const Crypto::Hash &blockHash,
        rapidjson::Writer<rapidjson::StringBuffer> &writer,
        const bool headerOnly = false);

    void generateTransactionPrefix(
        const CryptoNote::Transaction &tx,
        rapidjson::Writer<rapidjson::StringBuffer> &writer);

    /////////////////////
    /* OPTION REQUESTS */
    /////////////////////

    void handleOptions(const httplib::Request &req, httplib::Response &res) const;

    //////////////////
    /* GET REQUESTS */
    //////////////////

    std::tuple<Error, uint16_t>
        info(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        fee(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        height(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        peers(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getBlockCount(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getBlockHashForHeight(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getLastBlockHeader(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getBlockHeaderByHash(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getBlockHeaderByHeight(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getBlocksByHeight(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t> getTransactionDetailsByHash(
        const httplib::Request &req,
        httplib::Response &res,
        const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getTransactionsInPool(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getRawTransactionsInPool(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getRawBlockByHash(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getRawBlockByHeight(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getRawTransactionByHash(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    ///////////////////
    /* POST REQUESTS */
    ///////////////////

    std::tuple<Error, uint16_t>
        sendTransaction(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getRandomOuts(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getWalletSyncData(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getGlobalIndexes(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getTransactionsStatus(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getPoolChanges(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getRawBlocks(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        getBlockTemplate(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    std::tuple<Error, uint16_t>
        submitBlock(const httplib::Request &req, httplib::Response &res, const rapidjson::Document &body);

    //////////////////////////////
    /* Private member variables */
    //////////////////////////////

    /* Our server instance */
    httplib::Server m_server;

    /* The server host */
    const std::string m_host;

    /* The server port */
    const uint16_t m_port;

    /* The header to use with 'Access-Control-Allow-Origin'. If empty string,
     * header is not added. */
    const std::string m_corsHeader;

    /* The thread running the server */
    std::thread m_serverThread;

    /* The address to return from the /fee endpoint */
    const std::string m_feeAddress;

    /* The amount to return from the /fee endpoint */
    const uint64_t m_feeAmount;

    /* RPC methods that are enabled */
    const RpcMode m_rpcMode;

    /* A pointer to our CryptoNoteCore instance */
    const std::shared_ptr<CryptoNote::Core> m_core;

    /* A pointer to our P2P stack */
    const std::shared_ptr<CryptoNote::NodeServer> m_p2p;

    const std::shared_ptr<CryptoNote::ICryptoNoteProtocolHandler> m_syncManager;

    const std::string m_hashRegex = "[a-fA-F0-9]{64}";
};
