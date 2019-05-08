// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

////////////////////////
#include <Nigel/Nigel.h>
////////////////////////

#include <config/CryptoNoteConfig.h>

#include <Common/CryptoNoteTools.h>

#include <Errors/ValidateParameters.h>

#include <Logger/Logger.h>

#include <Utilities/Utilities.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"

////////////////////////////////
/*   Inline helper methods    */
////////////////////////////////

inline std::shared_ptr<httplib::Client> getClient(const std::string daemonHost, const uint16_t daemonPort, const bool daemonSSL, const std::chrono::seconds timeout)
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

Nigel::Nigel(
    const std::string daemonHost,
    const uint16_t daemonPort,
    const bool daemonSSL) :
    Nigel(daemonHost, daemonPort, daemonSSL, std::chrono::seconds(10))
{
}

Nigel::Nigel(
    const std::string daemonHost,
    const uint16_t daemonPort,
    const bool daemonSSL,
    const std::chrono::seconds timeout) :
    m_timeout(timeout),
    m_daemonHost(daemonHost),
    m_daemonPort(daemonPort),
    m_daemonSSL(daemonSSL)
{
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
    m_isBlockchainCache = false;

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
    for (const auto &item : blockHashCheckpoints) 
    {
        item.toJSON(writer);
    }
    writer.EndArray();
    writer.Key("startHeight");
    writer.Uint64(startHeight);
    writer.Key("startTimestamp");
    writer.Uint64(startTimestamp);
    writer.Key("blockCount");
    writer.Uint64(m_blockCount.load());

    Logger::logger.log(
        "Fetching blocks from the daemon",
        Logger::DEBUG,
        {Logger::SYNC, Logger::DAEMON}
    );

    auto res = m_nodeClient->Post(
        "/getwalletsyncdata", string_buffer.GetString(), "application/json"
    );

    if (res && res->status == 200) 
    {
        try 
        {
            rapidjson::Document j;
            j.Parse(res->body);

            if(j.HasParseError())
            {
                throw JsonException(rapidjson::GetParseError_En(j.GetParseError()));
            }

            if (getStringFromJSON(j, "status") != "OK")
            {
                return {false, {}};
            }
            
            std::vector<WalletTypes::WalletBlockInfo> items;
            // cooler way to get an array and iterate through it
            for (const auto& item : getArrayFromJSON(j, "items")) 
            {
                WalletTypes::WalletBlockInfo wbi;
                wbi.fromJSON(item);
                items.push_back(wbi);
            }
        } 
        catch (const JsonException &e) 
        {
            Logger::logger.log(
                std::string("Failed to fetch blocks from daemon: ") + "Unable to parse JSON (" +  
                e.what() + ")",
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

    auto res = m_nodeClient->Get("/info");

    if (res && res->status == 200)
    {
        try 
        {
            rapidjson::Document j;
            j.Parse(res->body);

            if(j.HasParseError())
            {
                throw JsonException(rapidjson::GetParseError_En(j.GetParseError()));
            }

            m_localDaemonBlockCount = getUint64FromJSON(j, "height");
            /* Height returned is one more than the current height - but we
            don't want to overflow is the height returned is zero */
            if (m_localDaemonBlockCount != 0) 
            {
                m_localDaemonBlockCount--;
            }

            m_networkBlockCount = getUint64FromJSON(j, "network_height");
            /* Height returned is one more than the current height - but we
            don't want to overflow is the height returned is zero */
            if (m_networkBlockCount != 0)
            {
                m_networkBlockCount--;
            }

            m_peerCount = getUint64FromJSON(j, "incoming_connections_count")
                        + getUint64FromJSON(j, "outgoing_connections_count");
            m_lastKnownHashrate = getUint64FromJSON(j, "difficulty")
                                / CryptoNote::parameters::DIFFICULTY_TARGET;

            /* Look to see if the isCacheApi property exists in the response
            and if so, set the internal value to whatever it found */
            if (j.HasMember("isCacheApi"))
            {
                m_isBlockchainCache = getBoolFromJSON(j, "isCacheApi");
            }
            return true;
        }
        catch (const JsonException &e) 
        {
            Logger::logger.log(
                std::string("Failed to update daemon info: ") + "Unable to parse JSON (" +  
                e.what() + ")",
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

    auto res = m_nodeClient->Get("/fee");

    if (res && res->status == 200)
    {
        try 
        {
            rapidjson::Document j;
            j.Parse(res->body);

            if(j.HasParseError())
            {
                throw JsonException(rapidjson::GetParseError_En(j.GetParseError()));
            }

            std::string tmpAddress = getStringFromJSON(j, "address");
            uint32_t tmpFee = getUintFromJSON(j, "amount");
            const bool integratedAddressesAllowed = false;
            Error error = validateAddresses({tmpAddress}, integratedAddressesAllowed);

            if (!error) 
            {
                m_nodeFeeAddress = tmpAddress;
                m_nodeFeeAmount = tmpFee;
            }

            return true;
        } 
        catch (const JsonException &e) 
        {
            Logger::logger.log(
                std::string("Failed to update fee info: ") + "Unable to parse JSON (" +  
                e.what() + ")",
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
    rapidjson::StringBuffer string_buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(string_buffer);

    writer.StartObject();
    writer.Key("transactionHashes");
    writer.StartArray();
    for (const auto& item : transactionHashes) 
    {
        item.toJSON(writer);
    }
    writer.EndArray();
    writer.EndObject();

    auto res = m_nodeClient->Post(
        "/get_transactions_status", string_buffer.GetString(), "application/json"
    );

    if (res && res->status == 200)
    {
        try 
        {
            rapidjson::Document j;
            j.Parse(res->body);

            if(j.HasParseError())
            {
                throw JsonException(rapidjson::GetParseError_En(j.GetParseError()));
            }

            if (getStringFromJSON(j, "status") != "OK")
            {
                return false;
            }

            transactionsInPool.clear();
            for (const auto& item : getArrayFromJSON(j, "transactionsInPool")) 
            {
                Crypto::Hash hash;
                hash.fromString(item.GetString());
                transactionsInPool.insert(hash);
            }
            transactionsInBlock.clear();
            for (const auto& item : getArrayFromJSON(j, "transactionsInBlock")) 
            {
                Crypto::Hash hash;
                hash.fromString(item.GetString());
                transactionsInBlock.insert(hash);
            }
            transactionsUnknown.clear();
            for (const auto& item : getArrayFromJSON(j, "transactionsUnknown")) 
            {
                Crypto::Hash hash;
                hash.fromString(item.GetString());
                transactionsUnknown.insert(hash);
            }

            return true;
        } 
        catch (const JsonException &e) 
        {
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
    for (const uint64_t item : amounts) 
    {
        writer.Uint64(item);
    }
    writer.EndArray();

    /* The blockchain cache doesn't call it outs_count
       it calls it mixin */
    if (m_isBlockchainCache) 
    {
        writer.Key("mixin");
        writer.Uint64(requestedOuts);
        writer.EndObject();

        /* We also need to handle the request and response a bit
           differently so we'll do this here */
        auto res = m_nodeClient->Post(
            "/randomOutputs", string_buffer.GetString(), "application/json"
        );

        if (res && res->status == 200) 
        {
            try 
            {
                rapidjson::Document j;
                j.Parse(res->body);

                if(j.HasParseError())
                {
                    throw JsonException(rapidjson::GetParseError_En(j.GetParseError()));
                }

                if (getStringFromJSON(j, "status") != "OK")
                {
                    return {};
                }

                std::vector<CryptoNote::RandomOuts> outs;
                for (const auto &x : getArrayFromJSON(j, "outs")) 
                {
                    CryptoNote::RandomOuts ro;
                    ro.fromJSON(x);
                    outs.push_back(ro);
                }

                return {true, outs};
            } catch (const JsonException &e) 
            {
            }
        }
    } 
    else 
    {
        writer.Key("outs_count");
        writer.Uint64(requestedOuts);
        writer.EndObject();

        auto res = m_nodeClient->Post(
            "/getrandom_outs", string_buffer.GetString(), "application/json"
        );

        if (res && res->status == 200) 
        {
            try 
            {
                rapidjson::Document j;
                j.Parse(res->body);

                if(j.HasParseError())
                {
                    throw JsonException(rapidjson::GetParseError_En(j.GetParseError()));
                }

                if (getStringFromJSON(j, "status") != "OK")
                {
                    return {};
                }

                std::vector<CryptoNote::RandomOuts> outs;
                for (const auto &x : getArrayFromJSON(j, "outs")) 
                {
                    CryptoNote::RandomOuts ro;
                    ro.fromJSON(x);
                    outs.push_back(ro);
                }

                return {true, outs};
            } 
            catch (const JsonException &e) 
            {
            }
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
    writer.String(Common::toHex(CryptoNote::toBinaryArray(tx)));
    writer.EndObject();

    auto res = m_nodeClient->Post(
        "/sendrawtransaction", string_buffer.GetString(), "application/json"
    );

    bool success = false;
    bool connectionError = true;

    if (res && res->status == 200)
    {
        connectionError = false;

        try 
        {
            rapidjson::Document j;
            j.Parse(res->body);
            if(j.HasParseError())
            {
                throw JsonException(rapidjson::GetParseError_En(j.GetParseError()));
            }

            if (getStringFromJSON(j, "status") != "OK")
            {
                return {};
            }
        } 
        catch (const JsonException &e) 
        {
        }
    }

    return {success, connectionError};
}

std::tuple<bool, std::unordered_map<Crypto::Hash, std::vector<uint64_t>>>
    Nigel::getGlobalIndexesForRange(
        const uint64_t startHeight,
        const uint64_t endHeight) const
{
    /* Blockchain cache API does not support this method and we
       don't need it to because it returns the global indexes
       with the key outputs when we get the wallet sync data */
    if (m_isBlockchainCache)
    {
      return {false, {}};
    }

    rapidjson::StringBuffer string_buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(string_buffer);

    writer.StartObject();
    writer.Key("startHeight");
    writer.Uint64(startHeight);
    writer.Key("endHeight");
    writer.Uint64(endHeight);
    writer.EndObject();

    auto res = m_nodeClient->Post(
        "/get_global_indexes_for_range", string_buffer.GetString(), "application/json"
    );

    if (res && res->status == 200)
    {
        try 
        {
            std::unordered_map<Crypto::Hash, std::vector<uint64_t>> result;
            rapidjson::Document j;
            j.Parse(res->body);
            if(j.HasParseError())
            {
                throw JsonException(rapidjson::GetParseError_En(j.GetParseError()));
            }

            if (getStringFromJSON(j, "status") != "OK")
            {
                return {false, {}};
            }

            std::vector<CryptoNote::RandomOuts> outs;
            for (const auto &index : getArrayFromJSON(j, "indexes")) 
            {
                // get hash
                Crypto::Hash hash;
                hash.fromString(getStringFromJSON(index, "key"));
                // get vector
                std::vector<uint64_t> vec;
                for (const auto& x : getArrayFromJSON(index, "value")) 
                {
                    vec.push_back(x.GetUint64());
                }
                // set result
                result[hash] = vec;
            }

            return {true, result};
        } 
        catch (const JsonException &e) 
        {
        }
    }

    return {false, {}};
}
