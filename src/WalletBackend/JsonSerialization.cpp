// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

////////////////////////////////////////////
#include <WalletBackend/JsonSerialization.h>
////////////////////////////////////////////

#include <Common/StringTools.h>

#include <tuple>

#include <SubWallets/SubWallets.h>

#include <WalletBackend/Constants.h>
#include <WalletBackend/SynchronizationStatus.h>
#include <WalletBackend/WalletBackend.h>
#include <WalletBackend/WalletSynchronizer.h>

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

std::vector<Transfer> transfersToVector(
    const std::unordered_map<Crypto::PublicKey, int64_t> transfers)
{
    std::vector<Transfer> vector;

    for (const auto & [publicKey, amount] : transfers)
    {
        Transfer t;
        t.publicKey = publicKey;
        t.amount = amount;

        vector.push_back(t);
    }

    return vector;
}

std::unordered_map<Crypto::PublicKey, int64_t> vectorToTransfers(
    const std::vector<Transfer> vector)
{
    std::unordered_map<Crypto::PublicKey, int64_t> transfers;

    for (const auto &transfer : vector)
    {
        transfers[transfer.publicKey] = transfer.amount;
    }

    return transfers;
}

std::vector<TxPrivateKey> txPrivateKeysToVector(
    const std::unordered_map<Crypto::Hash, Crypto::SecretKey> txPrivateKeys)
{
    std::vector<TxPrivateKey> vector;

    for (const auto [txHash, txPrivateKey] : txPrivateKeys)
    {
        vector.push_back({txHash, txPrivateKey});
    }

    return vector;
}

std::unordered_map<Crypto::Hash, Crypto::SecretKey> vectorToTxPrivateKeys(
    const std::vector<TxPrivateKey> vector)
{
    std::unordered_map<Crypto::Hash, Crypto::SecretKey> txPrivateKeys;

    for (const auto t : vector)
    {
        txPrivateKeys[t.txHash] = t.txPrivateKey;
    }

    return txPrivateKeys;
}
