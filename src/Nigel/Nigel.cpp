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

using json = nlohmann::json;

////////////////////////////////
/*   Inline helper methods    */
////////////////////////////////

inline std::shared_ptr<httplib::Client>
getClient(const std::string daemonHost, const uint16_t daemonPort, const bool daemonSSL,
          const std::chrono::seconds timeout)
{
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    if (daemonSSL)
    {
        return std::make_shared<httplib::SSLClient>(daemonHost.c_str(), daemonPort, timeout.count());
    } else
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

std::tuple<
        bool,
        std::vector<WalletTypes::WalletBlockInfo>,
        std::optional<WalletTypes::TopBlock>
> Nigel::getWalletSyncData(

        const std::vector<Crypto::Hash> blockHashCheckpoints,
        const uint64_t startHeight,
        const uint64_t startTimestamp,
        const bool skipCoinbaseTransactions) const
{
    Logger::logger.log(
            "Fetching blocks from the daemon",
            Logger::DEBUG,
            {Logger::SYNC, Logger::DAEMON}
    );

    json j = {
            {"blockHashCheckpoints",     blockHashCheckpoints},
            {"startHeight",              startHeight},
            {"startTimestamp",           startTimestamp},
            {"blockCount",               m_blockCount.load()},
            {"skipCoinbaseTransactions", skipCoinbaseTransactions}
    };

    auto res = m_nodeClient->Post(
            "/getwalletsyncdata", j.dump(), "application/json"
    );

    if (res && res->status == 200)
    {
        try
        {
            json j = json::parse(res->body);

            if (j.at("status").get<std::string>() != "OK")
            {
                return {false, {}, std::nullopt};
            }

            const auto items = j.at("items").get<std::vector<WalletTypes::WalletBlockInfo>>();

            if (j.find("synced") != j.end()
                && j.find("topBlock") != j.end()
                && j.at("synced").get<bool>())
            {
                return {
                        true,
                        items,
                        j.at("topBlock").get<WalletTypes::TopBlock>()
                };
            }

            return {true, items, std::nullopt};
        }
        catch (const json::exception &e)
        {
            Logger::logger.log(
                    std::string("Failed to fetch blocks from daemon: ") + e.what(),
                    Logger::INFO,
                    {Logger::SYNC, Logger::DAEMON}
            );
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
            json j = json::parse(res->body);

            m_localDaemonBlockCount = j.at("height").get<uint64_t>();

            /* Height returned is one more than the current height - but we
               don't want to overflow is the height returned is zero */
            if (m_localDaemonBlockCount != 0)
            {
                m_localDaemonBlockCount--;
            }

            m_networkBlockCount = j.at("network_height").get<uint64_t>();

            /* Height returned is one more than the current height - but we
               don't want to overflow is the height returned is zero */
            if (m_networkBlockCount != 0)
            {
                m_networkBlockCount--;
            }

            m_peerCount = j.at("incoming_connections_count").get<uint64_t>()
                          + j.at("outgoing_connections_count").get<uint64_t>();

            m_lastKnownHashrate = j.at("difficulty").get<uint64_t>()
                                  / CryptoNote::parameters::DIFFICULTY_TARGET;

            /* Look to see if the isCacheApi property exists in the response
               and if so, set the internal value to whatever it found */
            if (j.find("isCacheApi") != j.end())
            {
                m_isBlockchainCache = j.at("isCacheApi").get<bool>();
            }

            return true;
        }
        catch (const json::exception &e)
        {
            Logger::logger.log(
                    std::string("Failed to update daemon info: ") + e.what(),
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
            json j = json::parse(res->body);

            std::string tmpAddress = j.at("address").get<std::string>();

            uint32_t tmpFee = j.at("amount").get<uint32_t>();

            const bool integratedAddressesAllowed = false;

            Error error = validateAddresses({tmpAddress}, integratedAddressesAllowed);

            if (!error)
            {
                m_nodeFeeAddress = tmpAddress;
                m_nodeFeeAmount = tmpFee;
            }

            return true;
        }
        catch (const json::exception &e)
        {
            Logger::logger.log(
                    std::string("Failed to update fee info: ") + e.what(),
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
    json j = {
            {"transactionHashes", transactionHashes}
    };

    auto res = m_nodeClient->Post(
            "/get_transactions_status", j.dump(), "application/json"
    );

    if (res && res->status == 200)
    {
        try
        {
            json j = json::parse(res->body);

            if (j.at("status").get<std::string>() != "OK")
            {
                return false;
            }

            transactionsInPool = j.at("transactionsInPool").get<std::unordered_set<Crypto::Hash>>();
            transactionsInBlock = j.at("transactionsInBlock").get<std::unordered_set<Crypto::Hash>>();
            transactionsUnknown = j.at("transactionsUnknown").get<std::unordered_set<Crypto::Hash>>();
            return true;
        }
        catch (const json::exception &)
        {
        }
    }

    return false;
}

std::tuple<bool, std::vector<CryptoNote::RandomOuts>> Nigel::getRandomOutsByAmounts(
        const std::vector<uint64_t> amounts,
        const uint64_t requestedOuts) const
{
    json j = {
            {"amounts",    amounts},
            {"outs_count", requestedOuts}
    };

    /* The blockchain cache doesn't call it outs_count
       it calls it mixin */
    if (m_isBlockchainCache)
    {
        j.erase("outs_count");
        j["mixin"] = requestedOuts;

        /* We also need to handle the request and response a bit
           differently so we'll do this here */
        auto res = m_nodeClient->Post(
                "/randomOutputs", j.dump(), "application/json"
        );

        if (res && res->status == 200)
        {
            try
            {
                json j = json::parse(res->body);

                const auto outs = j.get<std::vector<CryptoNote::RandomOuts>>();

                return {true, outs};
            }
            catch (const json::exception &)
            {
            }
        }
    } else
    {
        auto res = m_nodeClient->Post(
                "/getrandom_outs", j.dump(), "application/json"
        );

        if (res && res->status == 200)
        {
            try
            {
                json j = json::parse(res->body);

                if (j.at("status").get<std::string>() != "OK")
                {
                    return {};
                }

                const auto outs = j.at("outs").get<std::vector<CryptoNote::RandomOuts>>();

                return {true, outs};
            }
            catch (const json::exception &)
            {
            }
        }
    }

    return {false, {}};
}

std::tuple<bool, bool> Nigel::sendTransaction(
        const CryptoNote::Transaction tx) const
{
    json j = {
            {"tx_as_hex", Common::toHex(CryptoNote::toBinaryArray(tx))}
    };

    auto res = m_nodeClient->Post(
            "/sendrawtransaction", j.dump(), "application/json"
    );

    bool success = false;
    bool connectionError = true;

    if (res && res->status == 200)
    {
        connectionError = false;

        try
        {
            json j = json::parse(res->body);

            success = j.at("status").get<std::string>() == "OK";
        }
        catch (const json::exception &)
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

    json j = {
            {"startHeight", startHeight},
            {"endHeight",   endHeight}
    };

    auto res = m_nodeClient->Post(
            "/get_global_indexes_for_range", j.dump(), "application/json"
    );

    if (res && res->status == 200)
    {
        try
        {
            std::unordered_map<Crypto::Hash, std::vector<uint64_t>> result;

            json j = json::parse(res->body);

            if (j.at("status").get<std::string>() != "OK")
            {
                return {false, {}};
            }

            /* The daemon doesn't serialize the way nlohmann::json does, so
               we can't just .get<std::unordered_map ...> */
            json indexes = j.at("indexes");

            for (const auto index : indexes)
            {
                result[index.at("key").get<Crypto::Hash>()] = index.at("value").get<std::vector<uint64_t>>();
            }

            return {true, result};
        }
        catch (const json::exception &)
        {
        }
    }

    return {false, {}};
}
