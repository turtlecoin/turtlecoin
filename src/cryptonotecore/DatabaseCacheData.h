// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cryptonotecore/BlockchainCache.h>
#include <map>

namespace CryptoNote
{
    struct KeyOutputInfo
    {
        Crypto::PublicKey publicKey;

        Crypto::Hash transactionHash;

        uint64_t unlockTime;

        uint16_t outputIndex;

        void serialize(CryptoNote::ISerializer &s);
    };

    // inherit here to avoid breaking IBlockchainCache interface
    struct ExtendedTransactionInfo : CachedTransactionInfo
    {
        // CachedTransactionInfo tx;
        std::map<IBlockchainCache::Amount, std::vector<IBlockchainCache::GlobalOutputIndex>>
            amountToKeyIndexes; // global key output indexes spawned in this transaction
        void serialize(ISerializer &s);
    };

} // namespace CryptoNote
