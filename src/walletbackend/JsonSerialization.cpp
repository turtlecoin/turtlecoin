// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

////////////////////////////////////////////
#include <walletbackend/JsonSerialization.h>
////////////////////////////////////////////

#include <common/StringTools.h>
#include <subwallets/SubWallets.h>
#include <tuple>
#include <walletbackend/Constants.h>
#include <walletbackend/SynchronizationStatus.h>
#include <walletbackend/WalletBackend.h>
#include <walletbackend/WalletSynchronizer.h>

using nlohmann::json;

namespace WalletTypes
{
    //////////////////////////////
    /* WalletTypes::Transaction */
    //////////////////////////////

    void to_json(json &j, const Transaction &t)
    {
        j = json {
            {"transfers", transfersToVector(t.transfers)},
            {"hash", t.hash},
            {"fee", t.fee},
            {"blockHeight", t.blockHeight},
            {"timestamp", t.timestamp},
            {"paymentID", t.paymentID},
            {"unlockTime", t.unlockTime},
            {"isCoinbaseTransaction", t.isCoinbaseTransaction},
        };
    }

    void from_json(const json &j, Transaction &t)
    {
        t.transfers = vectorToTransfers(j.at("transfers").get<std::vector<Transfer>>());
        t.hash = j.at("hash").get<Crypto::Hash>();
        t.fee = j.at("fee").get<uint64_t>();
        t.blockHeight = j.at("blockHeight").get<uint64_t>();
        t.timestamp = j.at("timestamp").get<uint64_t>();
        t.paymentID = j.at("paymentID").get<std::string>();
        t.unlockTime = j.at("unlockTime").get<uint64_t>();
        t.isCoinbaseTransaction = j.at("isCoinbaseTransaction").get<bool>();
    }
} // namespace WalletTypes

//////////////
/* Transfer */
//////////////

void to_json(json &j, const Transfer &t)
{
    j = {
        {"publicKey", t.publicKey},
        {"amount", t.amount},
    };
}

void from_json(const json &j, Transfer &t)
{
    t.publicKey = j.at("publicKey").get<Crypto::PublicKey>();
    t.amount = j.at("amount").get<int64_t>();
}

void to_json(json &j, const TxPrivateKey &t)
{
    j = {{"transactionHash", t.txHash}, {"txPrivateKey", t.txPrivateKey}};
}

void from_json(const json &j, TxPrivateKey &t)
{
    t.txHash = j.at("transactionHash").get<Crypto::Hash>();
    t.txPrivateKey = j.at("txPrivateKey").get<Crypto::SecretKey>();
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

std::vector<Transfer> transfersToVector(const std::unordered_map<Crypto::PublicKey, int64_t> transfers)
{
    std::vector<Transfer> vector;

    for (const auto &[publicKey, amount] : transfers)
    {
        Transfer t;
        t.publicKey = publicKey;
        t.amount = amount;

        vector.push_back(t);
    }

    return vector;
}

std::unordered_map<Crypto::PublicKey, int64_t> vectorToTransfers(const std::vector<Transfer> vector)
{
    std::unordered_map<Crypto::PublicKey, int64_t> transfers;

    for (const auto &transfer : vector)
    {
        transfers[transfer.publicKey] = transfer.amount;
    }

    return transfers;
}

std::vector<TxPrivateKey> txPrivateKeysToVector(const std::unordered_map<Crypto::Hash, Crypto::SecretKey> txPrivateKeys)
{
    std::vector<TxPrivateKey> vector;

    for (const auto [txHash, txPrivateKey] : txPrivateKeys)
    {
        vector.push_back({txHash, txPrivateKey});
    }

    return vector;
}

std::unordered_map<Crypto::Hash, Crypto::SecretKey> vectorToTxPrivateKeys(const std::vector<TxPrivateKey> vector)
{
    std::unordered_map<Crypto::Hash, Crypto::SecretKey> txPrivateKeys;

    for (const auto t : vector)
    {
        txPrivateKeys[t.txHash] = t.txPrivateKey;
    }

    return txPrivateKeys;
}
