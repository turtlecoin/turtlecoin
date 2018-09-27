// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoNote.h>

namespace WalletTypes
{
    struct KeyOutput
    {
        Crypto::PublicKey key;
        uint64_t amount;
    };

    /* A coinbase transaction (i.e., a miner reward, there is one of these in
       every block). Coinbase transactions have no inputs. 
       
       We call this a raw transaction, because it is simply key images and
       amounts */
    struct RawCoinbaseTransaction
    {
        /* The outputs of the transaction, amounts and keys */
        std::vector<KeyOutput> keyOutputs;

        /* The "extra" data, includes a pub key, sometimes payment ID's */
        std::vector<uint8_t> extra;

        /* The hash of the transaction */
        Crypto::Hash hash;
    };

    /* A raw transaction, simply key images and amounts */
    struct RawTransaction : RawCoinbaseTransaction
    {
        /* The inputs used for a transaction, can be used to track outgoing
           transactions */
        std::vector<CryptoNote::KeyInput> keyInputs;
    };

    /* A 'block' with the very basics needed to sync the transactions */
    struct WalletBlockInfo
    {
        /* The coinbase transaction */
        RawCoinbaseTransaction coinbaseTransaction;

        /* The transactions in the block */
        std::vector<RawTransaction> transactions;

        /* The block height (duh!) */
        uint64_t blockHeight = 0;

        /* The hash of the block */
        Crypto::Hash blockHash;
    };
}
