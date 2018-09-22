// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

////////////////////////////////////////////
#include <WalletBackend/JsonSerialization.h>
////////////////////////////////////////////

#include <WalletBackend/WalletBackend.h>
#include <WalletBackend/WalletSynchronizer.h>
#include <WalletBackend/SubWallet.h>
#include <WalletBackend/SynchronizationStatus.h>

using nlohmann::json;

///////////////
/* SubWallet */
///////////////

void to_json(json &j, const SubWallet &s)
{
    j = s.toJson();
}

void from_json(const json &j, SubWallet &s)
{
    s.fromJson(j);
}

/* We use member functions so we can access the private variables, and the
   functions above are defined so json() can auto convert between types */
json SubWallet::toJson() const
{
    return
    {
        {"privateSpendKey", m_privateSpendKey},
        {"address", m_address},
        {"syncStartTimestamp", m_syncStartTimestamp},
        {"transactions", m_transactions},
    };
}

void SubWallet::fromJson(const json &j)
{
    m_privateSpendKey = j.at("privateSpendKey").get<Crypto::SecretKey>();
    m_address = j.at("address").get<std::string>();
    m_syncStartTimestamp = j.at("syncStartTimestamp").get<uint64_t>();
    m_transactions = j.at("transactions").get<std::vector<CryptoNote::WalletTransaction>>();
}

///////////////////
/* WalletBackend */
///////////////////

void to_json(json &j, const WalletBackend &w)
{
    j = w.toJson();
}

void from_json(const json &j, WalletBackend &w)
{
    w.fromJson(j);
}

json WalletBackend::toJson() const
{
    return
    {
        {"walletFileFormatVersion", WALLET_FILE_FORMAT_VERSION},
        {"privateViewKey", m_privateViewKey},
        {"subWallets", m_subWallets},
        {"isViewWallet", m_isViewWallet},
        {"walletSynchronizer", *m_walletSynchronizer}
    };
}

void WalletBackend::fromJson(const json &j)
{
    uint16_t version = j.at("walletFileFormatVersion").get<uint16_t>();

    if (version != WALLET_FILE_FORMAT_VERSION)
    {
        /* TODO: This should probably be a custom type, throwing an actual
           error we can catch upstream? */
        throw std::invalid_argument(
            "Wallet file format version is not supported by this version of "
            "the software!"
        );
    }

    m_privateViewKey = j.at("privateViewKey").get<Crypto::SecretKey>();
    m_subWallets = j.at("subWallets").get<std::unordered_map<std::string, SubWallet>>();
    m_isViewWallet = j.at("isViewWallet").get<bool>();
    m_walletSynchronizer = std::make_shared<WalletSynchronizer>(j.at("walletSynchronizer").get<WalletSynchronizer>());
}

/* Declaration of to_json and from_json have to be in the same namespace as
   the type itself was declared in */
namespace Crypto
{
    ///////////////////////
    /* Crypto::SecretKey */
    ///////////////////////

    void to_json(json &j, const SecretKey &s)
    {
        hashToJson(j, s, "secretKey");
    }

    void from_json(const json &j, SecretKey &s)
    {
        jsonToHash(j, s, "secretKey");
    }

    //////////////////
    /* Crypto::Hash */
    //////////////////

    void to_json(json &j, const Hash &h)
    {
        hashToJson(j, h, "hash");
    }

    void from_json(const json &j, Hash &h)
    {
        jsonToHash(j, h, "hash");
    }
}

///////////////////////////////////
/* CryptoNote::WalletTransaction */
///////////////////////////////////

namespace CryptoNote
{
    void to_json(json &j, const WalletTransaction &t)
    {
        j = json {
            {"timestamp", t.timestamp},
            {"blockHeight", t.blockHeight},
            {"hash", t.hash},
            {"totalAmount", t.totalAmount},
            {"fee", t.fee},
        };
    }

    void from_json(const json &j, WalletTransaction &t)
    {
        t.timestamp = j.at("timestamp").get<uint64_t>();
        t.blockHeight = j.at("blockHeight").get<uint32_t>();
        t.hash = j.at("hash").get<Crypto::Hash>();
        t.totalAmount = j.at("totalAmount").get<int64_t>();
        t.fee = j.at("fee").get<uint64_t>();
    }
}

////////////////////////
/* WalletSynchronizer */
///////////////////////

void to_json(json &j, const WalletSynchronizer &w)
{
    j = w.toJson();
}

void from_json(const json &j, WalletSynchronizer &w)
{
    w.fromJson(j);
}

json WalletSynchronizer::toJson() const
{
    return
    {
        {"transactionSynchronizerStatus", m_transactionSynchronizerStatus},
        {"startTimestamp", m_startTimestamp},
        {"privateViewKey", m_privateViewKey}
    };
}

void WalletSynchronizer::fromJson(const json &j)
{
    m_blockDownloaderStatus = j.at("transactionSynchronizerStatus").get<SynchronizationStatus>();

    m_transactionSynchronizerStatus = m_blockDownloaderStatus;
    m_startTimestamp = j.at("startTimestamp").get<uint64_t>();
    m_privateViewKey = j.at("privateViewKey").get<Crypto::SecretKey>();
}

///////////////////////////
/* SynchronizationStatus */
///////////////////////////

void to_json(json &j, const SynchronizationStatus &s)
{
    j = s.toJson();
}

void from_json(const json &j, SynchronizationStatus &s)
{
    s.fromJson(j);
}

json SynchronizationStatus::toJson() const
{
    return
    {
        {"blockHashCheckpoints", m_blockHashCheckpoints},
        {"lastKnownBlockHashes", m_lastKnownBlockHashes},
        {"lastKnownBlockHeight", m_lastKnownBlockHeight}
    };
}

void SynchronizationStatus::fromJson(const json &j)
{
    m_blockHashCheckpoints = j.at("blockHashCheckpoints").get<std::deque<Crypto::Hash>>();
    m_lastKnownBlockHashes = j.at("lastKnownBlockHashes").get<std::deque<Crypto::Hash>>();
    m_lastKnownBlockHeight = j.at("lastKnownBlockHeight").get<uint64_t>();
}
