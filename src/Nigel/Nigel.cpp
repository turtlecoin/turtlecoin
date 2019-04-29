// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

////////////////////////
#include <Nigel/Nigel.h>
////////////////////////

#include <config/CryptoNoteConfig.h>

#include <CryptoNoteCore/CryptoNoteTools.h>

#include <Errors/ValidateParameters.h>

#include <Logger/Logger.h>

#include <Utilities/Utilities.h>

using json = nlohmann::json;

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"

////////////////////////////////
/* Constructors / Destructors */
////////////////////////////////

Nigel::Nigel(
    const std::string daemonHost, 
    const uint16_t daemonPort) : 
    Nigel(daemonHost, daemonPort, std::chrono::seconds(10))
{
}

Nigel::Nigel(
    const std::string daemonHost, 
    const uint16_t daemonPort,
    const std::chrono::seconds timeout) :
    m_timeout(timeout),
    m_daemonHost(daemonHost),
    m_daemonPort(daemonPort),
    m_httpClient(std::make_shared<httplib::Client>(daemonHost.c_str(), daemonPort, timeout.count()))
{
}

Nigel::~Nigel()
{
    stop();
}

//////////////////////
/* Member functions */
//////////////////////

void Nigel::swapNode(const std::string daemonHost, const uint16_t daemonPort)
{
    stop();

    m_localDaemonBlockCount = 0;
    m_networkBlockCount = 0;
    m_peerCount = 0;
    m_lastKnownHashrate = 0;

    m_daemonHost = daemonHost;
    m_daemonPort = daemonPort;

    m_httpClient = std::make_shared<httplib::Client>(
        daemonHost.c_str(), daemonPort, m_timeout.count()
    );

    init();
}

std::tuple<bool, std::vector<WalletTypes::WalletBlockInfo>> Nigel::getWalletSyncData(
    const std::vector<Crypto::Hash> blockHashCheckpoints,
    uint64_t startHeight,
    uint64_t startTimestamp) const
{
    rapidjson::StringBuffer string_buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(string_buffer);

    writer.StartObject();
    writer.Key("blockHashCheckpoints");
    writer.StartArray();
    for (const auto &item : blockHashCheckpoints) {
        item.toJSON(writer);
    }
    writer.EndArray();
    writer.Key("startHeight");
    writer.Uint64(startHeight);
    writer.Key("startTimestamp");
    writer.Uint64(startTimestamp);

    Logger::logger.log(
        "Fetching blocks from the daemon",
        Logger::DEBUG,
        {Logger::SYNC, Logger::DAEMON}
    );

    const auto res = m_httpClient->Post(
        "/getwalletsyncdata", string_buffer.GetString(), "application/json"
    );

    if (res && res->status == 200) {
        rapidjson::Document j;
        j.Parse(res->body);

        if(!j.HasParseError()) {
            if (j["status"].GetString() != "OK")
                return {false, {}};
            
            std::vector<WalletTypes::WalletBlockInfo> items;
            rapidjson::Value& value = j["items"];

            // cooler way to get an array and iterate through it
            for (auto& item : getArrayFromJSON(j, "items")) {
                WalletTypes::WalletBlockInfo wbi;
                wbi.fromJSON(item);
                items.push_back(wbi);
            }
        }
        else {
            Logger::logger.log(
                std::string("Failed to fetch blocks from daemon: ") + "Unable to parse JSON (" +  
                rapidjson::GetParseError_En(j.GetParseError()) + ")",
                Logger::INFO,
                {Logger::SYNC, Logger::DAEMON}
            );
        }
    }

    return {false, {}};
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
    Logger::logger.log(
        "Updating daemon info",
        Logger::DEBUG,
        {Logger::SYNC, Logger::DAEMON}
    );

    const auto res = m_httpClient->Get("/info");

    if (res && res->status == 200)
    {
        rapidjson::Document j;
        j.Parse(res->body);

        if(!j.HasParseError()) {
            m_localDaemonBlockCount = j["height"].GetUint64();
            /* Height returned is one more than the current height - but we
               don't want to overflow is the height returned is zero */
            if (m_localDaemonBlockCount != 0)
                m_localDaemonBlockCount--;

            m_networkBlockCount = j["network_height"].GetUint64();
            /* Height returned is one more than the current height - but we
               don't want to overflow is the height returned is zero */
            if (m_networkBlockCount != 0)
                m_networkBlockCount--;

            m_peerCount = j["incoming_connections_count"].GetUint64()
                        + j["outgoing_connections_count"].GetUint64();
            m_lastKnownHashrate = j["difficulty"].GetUint64()
                                / CryptoNote::parameters::DIFFICULTY_TARGET;
            return true;
        }
        else {
            Logger::logger.log(
                std::string("Failed to update daemon info: ") + "Unable to parse JSON (" +  
                rapidjson::GetParseError_En(j.GetParseError()) + ")",
                Logger::INFO,
                {Logger::SYNC, Logger::DAEMON}
            );
        }
    }

    return false;
}

bool Nigel::getFeeInfo()
{
    Logger::logger.log(
        "Fetching fee info",
        Logger::DEBUG,
        {Logger::DAEMON}
    );

    const auto res = m_httpClient->Get("/fee");

    if (res && res->status == 200)
    {
        rapidjson::Document j;
        j.Parse(res->body);

        if(!j.HasParseError()) {
            std::string tmpAddress = j["address"].GetString();
            uint32_t tmpFee = j["amount"].GetUint();
            const bool integratedAddressesAllowed = false;
            Error error = validateAddresses({tmpAddress}, integratedAddressesAllowed);

            if (!error) {
                m_nodeFeeAddress = tmpAddress;
                m_nodeFeeAmount = tmpFee;
            }

            return true;
        }
        else {
            Logger::logger.log(
                std::string("Failed to update fee info: ") + "Unable to parse JSON (" +  
                rapidjson::GetParseError_En(j.GetParseError()) + ")",
                Logger::INFO,
                {Logger::SYNC, Logger::DAEMON}
            );
        }
    }

    return false;
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
    return m_localDaemonBlockCount != 0 ||
           m_networkBlockCount != 0 ||
           m_peerCount != 0 ||
           m_lastKnownHashrate != 0;
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

std::tuple<std::string, uint16_t> Nigel::nodeAddress() const
{
    return {m_daemonHost, m_daemonPort};
}

bool Nigel::getTransactionsStatus(
    const std::unordered_set<Crypto::Hash> transactionHashes,
    std::unordered_set<Crypto::Hash> &transactionsInPool,
    std::unordered_set<Crypto::Hash> &transactionsInBlock,
    std::unordered_set<Crypto::Hash> &transactionsUnknown) const
{
    rapidjson::StringBuffer string_buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(string_buffer);

    writer.StartObject();
    writer.Key("transactionHashes");
    writer.StartArray();
    for (const auto& item : transactionHashes) {
        item.toJSON(writer);
    }
    writer.EndArray();
    writer.EndObject();

    const auto res = m_httpClient->Post(
        "/get_transactions_status", string_buffer.GetString(), "application/json"
    );

    if (res && res->status == 200)
    {
        rapidjson::Document j;
        j.Parse(res->body);

        if(!j.HasParseError()) {
            if (j["status"].GetString() != "OK")
                return false;

            transactionsInPool.clear();
            for (const auto& item : getArrayFromJSON(j, "transactionsInPool")) {
                Crypto::Hash hash;
                hash.fromString(item.GetString());
                transactionsInPool.insert(hash);
            }
            transactionsInBlock.clear();
            for (const auto& item : getArrayFromJSON(j, "transactionsInBlock")) {
                Crypto::Hash hash;
                hash.fromString(item.GetString());
                transactionsInBlock.insert(hash);
            }
            transactionsUnknown.clear();
            for (const auto& item : getArrayFromJSON(j, "transactionsUnknown")) {
                Crypto::Hash hash;
                hash.fromString(item.GetString());
                transactionsUnknown.insert(hash);
            }

            return true;
        }
        else {

        }
    }

    return false;
}

std::tuple<bool, std::vector<CryptoNote::RandomOuts>> Nigel::getRandomOutsByAmounts(
    const std::vector<uint64_t> amounts,
    const uint64_t requestedOuts) const
{
    rapidjson::StringBuffer string_buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(string_buffer);

    writer.StartObject();
    writer.Key("amounts");
    writer.StartArray();
    for (uint64_t item : amounts) {
        writer.Uint64(item);
    }
    writer.EndArray();
    writer.Key("outs_count");
    writer.Uint64(requestedOuts);
    writer.EndObject();

    const auto res = m_httpClient->Post(
        "/getrandom_outs", string_buffer.GetString(), "application/json"
    );

    if (res && res->status == 200)
    {
        rapidjson::Document j;
        j.Parse(res->body);

        if(!j.HasParseError()) {
            if (j["status"].GetString() != "OK")
                return {};

            std::vector<CryptoNote::RandomOuts> outs;
            for (const auto &x : getArrayFromJSON(j, "outs")) {
                CryptoNote::RandomOuts ro;
                ro.fromJSON(x);
                outs.push_back(ro);
            }

            return {true, outs};
        }
        else {

        }
    }

    return {false, {}};
}

std::tuple<bool, bool> Nigel::sendTransaction(
    const CryptoNote::Transaction tx) const
{
    rapidjson::StringBuffer string_buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(string_buffer);

    writer.StartObject();
    writer.Key("tx_as_hex");
    writer.StartArray();
    for (const auto& item : Common::toHex(CryptoNote::toBinaryArray(tx))) {
        writer.Uint(item);
    }
    writer.EndArray();
    writer.EndObject();

    const auto res = m_httpClient->Post(
        "/sendrawtransaction", string_buffer.GetString(), "application/json"
    );

    bool success = false;
    bool connectionError = true;

    if (res && res->status == 200)
    {
        connectionError = false;

        rapidjson::Document j;
        j.Parse(res->body);

        if(!j.HasParseError()) {
            if (j["status"].GetString() != "OK")
                return {};
        }
        else {

        }
    }

    return {success, connectionError};
}

std::tuple<bool, std::unordered_map<Crypto::Hash, std::vector<uint64_t>>>
    Nigel::getGlobalIndexesForRange(
        const uint64_t startHeight,
        const uint64_t endHeight) const
{
    rapidjson::StringBuffer string_buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(string_buffer);

    writer.StartObject();
    writer.Key("startHeight");
    writer.Uint64(startHeight);
    writer.Key("endHeight");
    writer.Uint64(endHeight);
    writer.EndObject();

    const auto res = m_httpClient->Post(
        "/get_global_indexes_for_range", string_buffer.GetString(), "application/json"
    );
    
    if (res && res->status == 200)
    {
        std::unordered_map<Crypto::Hash, std::vector<uint64_t>> result;
        rapidjson::Document j;
        j.Parse(res->body);

        if(!j.HasParseError()) {
            if (j["status"].GetString() != "OK")
                return {false, {}};

            std::vector<CryptoNote::RandomOuts> outs;
            for (const auto &index : getArrayFromJSON(j, "indexes")) {
                // get hash
                Crypto::Hash hash;
                hash.fromString(getStringFromJSON(j, "key"));
                // get vector
                std::vector<uint64_t> vec;
                for (const auto& x : getArrayFromJSON(j, "value")) {
                    vec.push_back(x.GetUint64());
                }
                // set result
                result[hash] = vec;
            }

            return {true, result};
        }
        else {

        }
    }

    return {false, {}};
}
