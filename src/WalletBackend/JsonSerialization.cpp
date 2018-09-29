// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

////////////////////////////////////////////
#include <WalletBackend/JsonSerialization.h>
////////////////////////////////////////////

#include <Common/StringTools.h>

#include <WalletBackend/Constants.h>
#include <WalletBackend/SubWallet.h>
#include <WalletBackend/SubWallets.h>
#include <WalletBackend/SynchronizationStatus.h>
#include <WalletBackend/WalletBackend.h>
#include <WalletBackend/WalletSynchronizer.h>

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
        {"publicSpendKey", m_publicSpendKey},
        {"privateSpendKey", m_privateSpendKey},
        {"address", m_address},
        {"syncStartTimestamp", m_syncStartTimestamp},
        {"isViewWallet", m_isViewWallet},
        {"keyImages", m_keyImages},
        {"balance", m_balance},
        {"syncStartHeight", m_syncStartHeight},
    };
}

void SubWallet::fromJson(const json &j)
{
    m_publicSpendKey = j.at("publicSpendKey").get<Crypto::PublicKey>();
    m_privateSpendKey = j.at("privateSpendKey").get<Crypto::SecretKey>();
    m_address = j.at("address").get<std::string>();
    m_syncStartTimestamp = j.at("syncStartTimestamp").get<uint64_t>();
    m_isViewWallet = j.at("isViewWallet").get<bool>();
    m_keyImages = j.at("keyImages").get<std::unordered_set<Crypto::KeyImage>>();
    m_balance = j.at("balance").get<uint64_t>();
    m_syncStartHeight = j.at("syncStartHeight").get<uint64_t>();
}

///////////////
/* SubWallets */
///////////////

void to_json(json &j, const SubWallets &s)
{
    j = s.toJson();
}

void from_json(const json &j, SubWallets &s)
{
    s.fromJson(j);
}

json SubWallets::toJson() const
{
    return
    {
        {"publicSpendKeys", m_publicSpendKeys},
        {"subWallets", m_subWallets},
        {"transactions", m_transactions},
    };
}

void SubWallets::fromJson(const json &j)
{
    m_publicSpendKeys = j.at("publicSpendKeys").get<std::vector<Crypto::PublicKey>>();
    m_subWallets = j.at("subWallets").get<std::unordered_map<Crypto::PublicKey, SubWallet>>();
    m_transactions = j.at("transactions").get<std::vector<Transaction>>();
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
        {"walletFileFormatVersion", Constants::WALLET_FILE_FORMAT_VERSION},
        {"privateViewKey", m_privateViewKey},
        {"subWallets", *m_subWallets},
        {"isViewWallet", m_isViewWallet},
        {"walletSynchronizer", *m_walletSynchronizer}
    };
}

void WalletBackend::fromJson(const json &j)
{
    uint16_t version = j.at("walletFileFormatVersion").get<uint16_t>();

    if (version != Constants::WALLET_FILE_FORMAT_VERSION)
    {
        /* TODO: This should probably be a custom type, throwing an actual
           error we can catch upstream? */
        throw std::invalid_argument(
            "Wallet file format version is not supported by this version of "
            "the software!"
        );
    }

    m_privateViewKey = j.at("privateViewKey").get<Crypto::SecretKey>();

    m_subWallets = std::make_shared<SubWallets>(
        j.at("subWallets").get<SubWallets>()
    );

    m_isViewWallet = j.at("isViewWallet").get<bool>();

    m_walletSynchronizer = std::make_shared<WalletSynchronizer>(
        j.at("walletSynchronizer").get<WalletSynchronizer>()
    );
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

    ///////////////////////
    /* Crypto::PublicKey */
    ///////////////////////

    void to_json(json &j, const PublicKey &s)
    {
        hashToJson(j, s, "publicKey");
    }

    void from_json(const json &j, PublicKey &s)
    {
        jsonToHash(j, s, "publicKey");
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

    //////////////////////
    /* Crypto::KeyImage */
    //////////////////////

    void to_json(json &j, const KeyImage &h)
    {
        hashToJson(j, h, "hash");
    }

    void from_json(const json &j, KeyImage &h)
    {
        jsonToHash(j, h, "hash");
    }
}

/////////////////
/* Transaction */
/////////////////

void to_json(json &j, const Transaction &t)
{
    j = json {
        {"transfers", t.transfers},
        {"hash", t.hash},
        {"fee", t.fee},
        {"timestamp", t.timestamp},
        {"blockHeight", t.blockHeight},
        {"paymentID", t.paymentID},
    };
}

void from_json(const json &j, Transaction &t)
{
    t.transfers = j.at("transfers").get<std::unordered_map<Crypto::PublicKey, int64_t>>();
    t.hash = j.at("hash").get<Crypto::Hash>();
    t.fee = j.at("fee").get<uint64_t>();
    t.timestamp = j.at("timestamp").get<uint64_t>();
    t.blockHeight = j.at("blockHeight").get<uint32_t>();
    t.paymentID = j.at("paymentID").get<std::string>();
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
        {"startHeight", m_startHeight},
        {"privateViewKey", m_privateViewKey}
    };
}

void WalletSynchronizer::fromJson(const json &j)
{
    m_blockDownloaderStatus = j.at("transactionSynchronizerStatus").get<SynchronizationStatus>();

    m_transactionSynchronizerStatus = m_blockDownloaderStatus;

    m_startTimestamp = j.at("startTimestamp").get<uint64_t>();
    m_startHeight = j.at("startHeight").get<uint64_t>();

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
