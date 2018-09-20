// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <INode.h>

/* TODO: Just return these types from queryBlocks() */
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
    std::vector<KeyOutput> keyOutputs;
    std::vector<uint8_t> extra;
};

/* A raw transaction, simply key images and amounts */
struct RawTransaction : RawCoinbaseTransaction
{
    std::vector<CryptoNote::KeyInput> keyInputs;
};

/* A 'block' with the very basics needed to sync the transactions */
struct RawBlock
{
    RawBlock(RawCoinbaseTransaction coinbaseTransaction,
             std::vector<RawTransaction> transactions,
             uint64_t blockHeight,
             Crypto::Hash blockHash) :
        coinbaseTransaction(coinbaseTransaction),
        transactions(transactions),
        blockHeight(blockHeight),
        blockHash(blockHash) {};

    /* The coinbase transaction */
    RawCoinbaseTransaction coinbaseTransaction;

    /* The transactions in the block */
    std::vector<RawTransaction> transactions;

    /* The block height (duh!) */
    uint64_t blockHeight;

    /* The hash of the block */
    Crypto::Hash blockHash;
};

RawCoinbaseTransaction getRawCoinbaseTransaction(CryptoNote::Transaction t);

RawTransaction getRawTransaction(CryptoNote::TransactionPrefix t);
