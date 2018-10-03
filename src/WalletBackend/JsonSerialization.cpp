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
    Common::podFromHex(j.at("publicSpendKey").get<std::string>(), m_publicSpendKey.data);
    Common::podFromHex(j.at("privateSpendKey").get<std::string>(), m_privateSpendKey.data);
    m_address = j.at("address").get<std::string>();
    m_syncStartTimestamp = j.at("syncStartTimestamp").get<uint64_t>();
    m_isViewWallet = j.at("isViewWallet").get<bool>();

    /* Temporary work around whilst we wait for a fix for
       https://github.com/nlohmann/json/issues/1267 - parse it as a vector
       of strings then convert to a set of key images 

    m_keyImages = j.at("keyImages").get<std::unordered_set<Crypto::KeyImage>>();

    */

    m_keyImages = vectorToContainer<std::unordered_set<Crypto::KeyImage>>(j.at("keyImages").get<std::vector<std::string>>());
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
        {"subWallet", subWalletsToVector(m_subWallets)},
        {"transactions", m_transactions},
    };
}

void SubWallets::fromJson(const json &j)
{
    m_publicSpendKeys = vectorToContainer<std::vector<Crypto::PublicKey>>(j.at("publicSpendKeys").get<std::vector<std::string>>());
    m_subWallets = vectorToSubWallets(j.at("subWallet").get<std::vector<SubWallet>>());
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

    Common::podFromHex(j.at("privateViewKey").get<std::string>(), m_privateViewKey.data);

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
        j = Common::podToHex(s);
    }

    ///////////////////////
    /* Crypto::PublicKey */
    ///////////////////////

    void to_json(json &j, const PublicKey &s)
    {
        j = Common::podToHex(s);
    }

    //////////////////
    /* Crypto::Hash */
    //////////////////

    void to_json(json &j, const Hash &h)
    {
        j = Common::podToHex(h);
    }

    //////////////////////
    /* Crypto::KeyImage */
    //////////////////////

    void to_json(json &j, const KeyImage &h)
    {
        j = Common::podToHex(h);
    }
}

/////////////////
/* Transaction */
/////////////////

void to_json(json &j, const Transaction &t)
{
    j = json {
        {"transfers", transfersToVector(t.transfers)},
        {"hash", t.hash},
        {"fee", t.fee},
        {"timestamp", t.timestamp},
        {"blockHeight", t.blockHeight},
        {"paymentID", t.paymentID},
    };
}

void from_json(const json &j, Transaction &t)
{
    t.transfers = vectorToTransfers(j.at("transfers").get<std::vector<Transfer>>());
    Common::podFromHex(j.at("hash").get<std::string>(), t.hash.data);
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

    Common::podFromHex(j.at("privateViewKey").get<std::string>(), m_privateViewKey.data);
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
    m_blockHashCheckpoints = vectorToContainer<std::deque<Crypto::Hash>>(j.at("blockHashCheckpoints").get<std::vector<std::string>>());
    m_lastKnownBlockHashes = vectorToContainer<std::deque<Crypto::Hash>>(j.at("lastKnownBlockHashes").get<std::vector<std::string>>());
    m_lastKnownBlockHeight = j.at("lastKnownBlockHeight").get<uint64_t>();
}

//////////////
/* Transfer */
//////////////

void to_json(json &j, const Transfer &t)
{
    j = {
        {"publicKey", Common::podToHex(t.publicKey)},
        {"amount", t.amount},
    };
}

void from_json(const json &j, Transfer &t)
{
    Common::podFromHex(j.at("publicKey").get<std::string>(), t.publicKey.data);
    t.amount = j.at("amount").get<int64_t>();
}

/* std::map / std::unordered_map don't work great in json - they get serialized
   like this for example: 

"transfers": [
    [
          {
                "publicKey": "95b86472ef19dcd7e787031ae4e749226c4ea672c8d41ca960cc1b8cd7d4766f"
          },
          100
    ]
]

This is not very helpful, as it's not obvious that '100' is the amount, and
it's not easy to access, as there's no key to access it by.

So, lets instead convert to a vector of structs when converting to json, to
make it easier for people using the wallet file in different languages to
use */

std::vector<Transfer> transfersToVector(std::unordered_map<Crypto::PublicKey, int64_t> transfers)
{
    std::vector<Transfer> vector;

    for (const auto &x : transfers)
    {
        Transfer t;
        t.publicKey = x.first;
        t.amount = x.second;

        vector.push_back(t);
    }

    return vector;
}

std::unordered_map<Crypto::PublicKey, int64_t> vectorToTransfers(std::vector<Transfer> vector)
{
    std::unordered_map<Crypto::PublicKey, int64_t> transfers;

    for (const auto &t : vector)
    {
        transfers[t.publicKey] = t.amount;
    }

    return transfers;
}

std::vector<SubWallet> subWalletsToVector(std::unordered_map<Crypto::PublicKey, SubWallet> subWallets)
{
    std::vector<SubWallet> vector;

    for (const auto &x : subWallets)
    {
        vector.push_back(x.second);
    }

    return vector;
}

std::unordered_map<Crypto::PublicKey, SubWallet> vectorToSubWallets(std::vector<SubWallet> vector)
{
    std::unordered_map<Crypto::PublicKey, SubWallet> transfers;

    for (const auto &s : vector)
    {
        transfers[s.m_publicSpendKey] = s;
    }

    return transfers;
}
