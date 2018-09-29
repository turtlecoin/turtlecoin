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

        /* The hash of the transaction */
        Crypto::Hash hash;

        /* The public key of this transaction, taken from the tx extra */
        Crypto::PublicKey transactionPublicKey;
    };

    /* A raw transaction, simply key images and amounts */
    struct RawTransaction : RawCoinbaseTransaction
    {
        /* The transaction payment ID - may be an empty string */
        std::string paymentID;

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
        uint64_t blockHeight;

        /* The hash of the block */
        Crypto::Hash blockHash;

        /* The timestamp of the block */
        uint64_t blockTimestamp;
    };
}
