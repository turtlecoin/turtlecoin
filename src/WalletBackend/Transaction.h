// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

struct Transaction
{
    Transaction() {};

    Transaction(std::unordered_map<Crypto::PublicKey, int64_t> transfers,
                Crypto::Hash hash,
                uint64_t fee) :
        transfers(transfers),
        hash(hash),
        fee(fee)
    {
    }

    /* A map of public keys to amounts, since one transaction can go to
       multiple addresses. These can be positive or negative, for example
       one address might have sent 10,000 TRTL (-10000) to two recipients
       (+5000), (+5000) 
       
       All the public keys in this map, are ones that the wallet container
       owns, it won't store amounts belonging to random people */
    std::unordered_map<Crypto::PublicKey, int64_t> transfers;

    /* The hash of the transaction */
    Crypto::Hash hash;

    /* The fee the transaction was sent with (always positive) */
    uint64_t fee;
};
