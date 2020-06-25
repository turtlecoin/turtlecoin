// Copyright (c) 2018-2020, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

////////////////////////
#include <nigel/Nigel.h>
////////////////////////

#include <CryptoNote.h>
#include <common/CryptoNoteTools.h>
#include <config/CryptoNoteConfig.h>
#include <cryptonotecore/CachedBlock.h>
#include <cryptonotecore/Core.h>
#include <errors/ValidateParameters.h>
#include <utilities/Utilities.h>
#include <version.h>

////////////////////////////////
/*   Inline helper methods    */
////////////////////////////////

inline std::shared_ptr<httplib::Client> getClient(
    const std::string daemonHost,
    const uint16_t daemonPort,
    const bool daemonSSL,
    const std::chrono::seconds timeout)
{
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    if (daemonSSL)
    {
        return std::make_shared<httplib::SSLClient>(daemonHost.c_str(), daemonPort, timeout.count());
    }
    else
    {
#endif
        return std::make_shared<httplib::Client>(daemonHost.c_str(), daemonPort, timeout.count());
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    }
#endif
}

////////////////////////////////
/* Constructors / Destructors */
////////////////////////////////

Nigel::Nigel(const std::string daemonHost, const uint16_t daemonPort, const bool daemonSSL):
    Nigel(daemonHost, daemonPort, daemonSSL, std::chrono::seconds(10))
{
}

Nigel::Nigel(
    const std::string daemonHost,
    const uint16_t daemonPort,
    const bool daemonSSL,
    const std::chrono::seconds timeout):
    m_timeout(timeout),
    m_daemonHost(daemonHost),
    m_daemonPort(daemonPort),
    m_daemonSSL(daemonSSL)
{
    std::stringstream userAgent;
    userAgent << "Nigel/" << PROJECT_VERSION_LONG;

    m_requestHeaders = {{"User-Agent", userAgent.str()}};
    m_nodeClient = getClient(m_daemonHost, m_daemonPort, m_daemonSSL, m_timeout);
}

Nigel::~Nigel()
{
    stop();
}

//////////////////////
/* Member functions */
//////////////////////

void Nigel::swapNode(const std::string daemonHost, const uint16_t daemonPort, const bool daemonSSL)
{
    stop();

    m_blockCount = CryptoNote::BLOCKS_SYNCHRONIZING_DEFAULT_COUNT;
    m_localDaemonBlockCount = 0;
    m_networkBlockCount = 0;
    m_peerCount = 0;
    m_lastKnownHashrate = 0;
    m_nodeFeeAddress = "";
    m_nodeFeeAmount = 0;

    m_daemonHost = daemonHost;
    m_daemonPort = daemonPort;
    m_daemonSSL = daemonSSL;

    m_nodeClient = getClient(m_daemonHost, m_daemonPort, m_daemonSSL, m_timeout);

    init();
}

void Nigel::decreaseRequestedBlockCount()
{
    if (m_blockCount > 1)
    {
        m_blockCount = m_blockCount / 2;
    }
}

void Nigel::resetRequestedBlockCount()
{
    m_blockCount = CryptoNote::BLOCKS_SYNCHRONIZING_DEFAULT_COUNT;
}

std::tuple<bool, std::vector<WalletTypes::WalletBlockInfo>, std::optional<WalletTypes::TopBlock>>
    Nigel::getWalletSyncData(
        const std::vector<Crypto::Hash> blockHashCheckpoints,
        const uint64_t startHeight,
        const uint64_t startTimestamp,
        const bool skipCoinbaseTransactions)
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    Logger::logger.log("Fetching blocks from the daemon", Logger::DEBUG, {Logger::SYNC, Logger::DAEMON});

    writer.StartObject();
    {
        writer.Key("checkpoints");
        writer.StartArray();
        {
            for (const auto &hash : blockHashCheckpoints)
            {
                hash.toJSON(writer);
            }
        }
        writer.EndArray();

        writer.Key("height");
        writer.Uint64(startHeight);

        writer.Key("timestamp");
        writer.Uint64(startTimestamp);

        writer.Key("count");
        writer.Uint64(m_blockCount.load());

        writer.Key("skipCoinbaseTransactions");
        writer.Bool(skipCoinbaseTransactions);
    }
    writer.EndObject();

    const std::string dump(sb.GetString(), sb.GetLength());

    Logger::logger.log(
        "Sending /sync/raw request to daemon: " + dump, Logger::TRACE, {Logger::SYNC, Logger::DAEMON});

    const auto res = m_nodeClient->Post("/sync/raw", m_requestHeaders, sb.GetString(), "application/json");

    const auto body = getJsonBody(res, "Failed to fetch blocks from daemon");

    if (body)
    {
        if (!hasMember(body.value(), "error"))
        {
            std::vector<WalletTypes::WalletBlockInfo> items;

            for (const auto &block : getArrayFromJSON(body.value(), "blocks"))
            {
                CryptoNote::RawBlock rawBlock;

                rawBlock.fromJSON(block);

                CryptoNote::BlockTemplate blockTemplate;

                fromBinaryArray(blockTemplate, rawBlock.block);

                WalletTypes::WalletBlockInfo walletBlock;

                CryptoNote::CachedBlock cachedBlock(blockTemplate);

                walletBlock.blockHeight = cachedBlock.getBlockIndex();

                walletBlock.blockHash = cachedBlock.getBlockHash();

                walletBlock.blockTimestamp = blockTemplate.timestamp;

                if (!skipCoinbaseTransactions)
                {
                    walletBlock.coinbaseTransaction =
                        CryptoNote::Core::getRawCoinbaseTransaction(blockTemplate.baseTransaction);
                }

                for (const auto &transaction : rawBlock.transactions)
                {
                    walletBlock.transactions.push_back(CryptoNote::Core::getRawTransaction(transaction));
                }

                items.push_back(walletBlock);
            }

            std::optional<WalletTypes::TopBlock> topBlock;

            if (hasMember(body.value(),"synced") && hasMember(body.value(),"topBlock"))
            {
                if (getBoolFromJSON(body.value(), "synced"))
                {
                    WalletTypes::TopBlock tmpTopBlock;

                    tmpTopBlock.fromJSON(getJsonValue(body.value(), "topBlock"));

                    topBlock = tmpTopBlock;
                }
            }

            return {true, items, topBlock};
        }
    }

    return {false, {}, std::nullopt};
}

void Nigel::stop()
{
    m_shouldStop = true;

    if (m_backgroundThread.joinable())
    {
        m_backgroundThread.join();
    }
}

void Nigel::init()
{
    m_shouldStop = false;

    /* Get the initial daemon info, and the initial fee info before returning.
       This way the info is always valid, and there's no race on accessing
       the fee info or something */
    getDaemonInfo();

    getFeeInfo();

    /* Now launch the background thread to constantly update the heights etc */
    m_backgroundThread = std::thread(&Nigel::backgroundRefresh, this);
}

bool Nigel::getDaemonInfo()
{
    Logger::logger.log("Updating daemon info", Logger::DEBUG, {Logger::SYNC, Logger::DAEMON});

    Logger::logger.log("Sending /info request to daemon", Logger::TRACE, {Logger::SYNC, Logger::DAEMON});

    auto res = m_nodeClient->Get("/info", m_requestHeaders);

    const auto body = getJsonBody(res, "Failed to  update daemon info");

    if (body)
    {
        if (hasMember(body.value(), "height"))
        {
            m_localDaemonBlockCount = getUint64FromJSON(body.value(), "height");

            if (m_localDaemonBlockCount != 0)
            {
                m_localDaemonBlockCount--;
            }
        }

        if (hasMember(body.value(), "networkHeight"))
        {
            m_networkBlockCount = getUint64FromJSON(body.value(), "networkHeight");

            if (m_networkBlockCount != 0)
            {
                m_networkBlockCount--;
            }
        }

        if (hasMember(body.value(), "incomingConnections")
            && hasMember(body.value(), "outgoingConnections"))
        {
            m_peerCount = getUint64FromJSON(body.value(), "incomingConnections")
                          + getUint64FromJSON(body.value(), "outgoingConnections");
        }

        if (hasMember(body.value(), "hashrate"))
        {
            m_lastKnownHashrate = getUint64FromJSON(body.value(), "hashrate");
        }
    }

    return body.has_value();
}

bool Nigel::getFeeInfo()
{
    Logger::logger.log("Fetching fee info", Logger::DEBUG, {Logger::DAEMON});

    Logger::logger.log("Sending /fee request to daemon", Logger::TRACE, {Logger::SYNC, Logger::DAEMON});

    auto res = m_nodeClient->Get("/fee", m_requestHeaders);

    const auto body = getJsonBody(res, "Failed to update fee information");

    if (body)
    {
        if (!hasMember(body.value(), "error"))
        {
            const std::string tmpAddress = getStringFromJSON(body.value(), "address");

            const uint32_t tmpFee = getUintFromJSON(body.value(), "amount");

            const bool integratedAddressesAllowed = false;

            Error error = validateAddresses({tmpAddress}, integratedAddressesAllowed);

            if (!error)
            {
                m_nodeFeeAddress = tmpAddress;

                m_nodeFeeAmount = tmpFee;
            }

            return true;
        }
    }

    return body.has_value();
}

void Nigel::backgroundRefresh()
{
    while (!m_shouldStop)
    {
        getDaemonInfo();

        Utilities::sleepUnlessStopping(std::chrono::seconds(10), m_shouldStop);
    }
}

bool Nigel::isOnline() const
{
    return m_localDaemonBlockCount != 0 || m_networkBlockCount != 0 || m_peerCount != 0 || m_lastKnownHashrate != 0;
}

uint64_t Nigel::localDaemonBlockCount() const
{
    return m_localDaemonBlockCount;
}

uint64_t Nigel::networkBlockCount() const
{
    return m_networkBlockCount;
}

uint64_t Nigel::peerCount() const
{
    return m_peerCount;
}

uint64_t Nigel::hashrate() const
{
    return m_lastKnownHashrate;
}

std::tuple<uint64_t, std::string> Nigel::nodeFee() const
{
    return {m_nodeFeeAmount, m_nodeFeeAddress};
}

std::tuple<std::string, uint16_t, bool> Nigel::nodeAddress() const
{
    return {m_daemonHost, m_daemonPort, m_daemonSSL};
}

bool Nigel::getTransactionsStatus(
    const std::unordered_set<Crypto::Hash> transactionHashes,
    std::unordered_set<Crypto::Hash> &transactionsInPool,
    std::unordered_set<Crypto::Hash> &transactionsInBlock,
    std::unordered_set<Crypto::Hash> &transactionsUnknown) const
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartArray();
    {
        for (const auto &hash : transactionHashes)
        {
            hash.toJSON(writer);
        }
    }
    writer.EndArray();

    const std::string dump(sb.GetString(), sb.GetLength());

    Logger::logger.log(
        "Sending /transaction/status request to daemon: " + dump, Logger::TRACE, {Logger::SYNC, Logger::DAEMON});

    auto res = m_nodeClient->Post("/transaction/status", m_requestHeaders, sb.GetString(), "application/json");

    const auto body = getJsonBody(res, "Failed to get transactions status");

    if (body)
    {
        if (!hasMember(body.value(), "error"))
        {
            transactionsInPool.clear();

            if (hasMember(body.value(), "inPool"))
            {
                for (const auto &val : getArrayFromJSON(body.value(), "inPool"))
                {
                    Crypto::Hash hash;

                    hash.fromJSON(val);

                    transactionsInPool.insert(hash);
                }
            }

            transactionsInBlock.clear();

            if (hasMember(body.value(), "inBlock"))
            {
                for (const auto &val : getArrayFromJSON(body.value(), "inBlock"))
                {
                    Crypto::Hash hash;

                    hash.fromJSON(val);

                    transactionsInBlock.insert(hash);
                }
            }

            transactionsUnknown.clear();

            if (hasMember(body.value(), "notFound"))
            {
                for (const auto &val : getArrayFromJSON(body.value(), "notFound"))
                {
                    Crypto::Hash hash;

                    hash.fromJSON(val);

                    transactionsUnknown.insert(hash);
                }
            }
        }
    }

    return body.has_value();
}

std::tuple<bool, std::vector<WalletTypes::RandomOuts>>
    Nigel::getRandomOutsByAmounts(const std::vector<uint64_t> amounts, const uint64_t requestedOuts) const
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    {
        writer.Key("amounts");
        writer.StartArray();
        {
            for (const auto &amount : amounts)
            {
                writer.Uint64(amount);
            }
        }
        writer.EndArray();

        writer.Key("count");
        writer.Uint64(requestedOuts);
    }
    writer.EndObject();

    const std::string dump(sb.GetString(), sb.GetLength());

    Logger::logger.log(
        "Sending /indexes/random request to daemon: " + dump, Logger::TRACE, {Logger::SYNC, Logger::DAEMON});

    auto res = m_nodeClient->Post("/indexes/random", m_requestHeaders, sb.GetString(), "application/json");

    const auto body = getJsonBody(res, "Failed to get random outputs");

    if (body)
    {
        if (!hasMember(body.value(), "error"))
        {
            std::vector<WalletTypes::RandomOuts> outputs;

            for (const auto &randomOutput : getArrayFromJSON(body.value()))
            {
                WalletTypes::RandomOuts output;

                output.fromJSON(randomOutput);

                outputs.push_back(output);
            }

            return {true, outputs};
        }
    }

    return {false, {}};
}

std::tuple<bool, bool, std::string> Nigel::sendTransaction(const CryptoNote::Transaction tx) const
{
    rapidjson::StringBuffer sb;

    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.String(Common::toHex(CryptoNote::toBinaryArray(tx)));

    const std::string dump(sb.GetString(), sb.GetLength());

    Logger::logger.log(
        "Sending /transaction request to daemon: " + dump, Logger::TRACE, {Logger::SYNC, Logger::DAEMON});

    auto res = m_nodeClient->Post("/transaction", m_requestHeaders, sb.GetString(), "application/json");

    std::string error;

    /* If we received a 202 back, then the transaction was accepted by the daemon */
    if (res->status == 202)
    {
        return {true, false, error};
    }

    bool connectionError = true;

    const auto body = getJsonBody(res, "Failed to send transaction");

    if (body)
    {
        connectionError = false;

        if (hasMember(body.value(), "error"))
        {
            const auto errorObject = getObjectFromJSON(body.value(), "error");

            error = getStringFromJSON(errorObject, "message");
        }
    }

    return {false, connectionError, error};
}

std::tuple<bool, std::unordered_map<Crypto::Hash, std::vector<uint64_t>>>
    Nigel::getGlobalIndexesForRange(const uint64_t startHeight, const uint64_t endHeight) const
{
    Logger::logger.log(
        "Sending /indexes/" + std::to_string(startHeight) + "/" + std::to_string(endHeight) +
            "request to daemon",
        Logger::TRACE,
        {Logger::SYNC, Logger::DAEMON});

    auto res = m_nodeClient->Get(
        "/indexes" + std::to_string(startHeight) + "/" + std::to_string(endHeight),
        m_requestHeaders);

    std::unordered_map<Crypto::Hash, std::vector<uint64_t>> result;

    const auto body = getJsonBody(res, "Failed to get global indexes for range");

    bool success = true;

    if (body)
    {
        if (hasMember(body.value(), "error"))
        {
            success = false;
        }
        else
        {
            for (const auto &tx : getArrayFromJSON(body.value()))
            {
                Crypto::Hash hash;

                hash.fromJSON(getJsonValue(tx, "hash"));

                std::vector<uint64_t> indexes;

                for (const auto &index : getArrayFromJSON(tx, "indexes"))
                {
                    indexes.push_back(getUint64FromJSON(index));
                }

                result[hash] = indexes;
            }
        }
    }

    return {success, result};
}
