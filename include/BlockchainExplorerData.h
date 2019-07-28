// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoNote.h"
#include "CryptoTypes.h"

#include <array>
#include <boost/variant.hpp>
#include <string>
#include <vector>

namespace CryptoNote
{
    enum class TransactionRemoveReason : uint8_t
    {
        INCLUDED_IN_BLOCK = 0,
        TIMEOUT = 1
    };

    struct TransactionOutputDetails
    {
        TransactionOutput output;
        uint64_t globalIndex;
    };

    struct TransactionOutputReferenceDetails
    {
        Crypto::Hash transactionHash;
        uint64_t number;
    };

    struct BaseInputDetails
    {
        BaseInput input;
        uint64_t amount;
    };

    struct KeyInputDetails
    {
        KeyInput input;
        uint64_t mixin;
        TransactionOutputReferenceDetails output;
    };

    typedef boost::variant<BaseInputDetails, KeyInputDetails> TransactionInputDetails;

    struct TransactionExtraDetails
    {
        Crypto::PublicKey publicKey;
        BinaryArray nonce;
        BinaryArray raw;
    };

    struct TransactionDetails
    {
        Crypto::Hash hash;
        uint64_t size = 0;
        uint64_t fee = 0;
        uint64_t totalInputsAmount = 0;
        uint64_t totalOutputsAmount = 0;
        uint64_t mixin = 0;
        uint64_t unlockTime = 0;
        uint64_t timestamp = 0;
        Crypto::Hash paymentId;
        bool hasPaymentId = false;
        bool inBlockchain = false;
        Crypto::Hash blockHash;
        uint32_t blockIndex = 0;
        TransactionExtraDetails extra;
        std::vector<std::vector<Crypto::Signature>> signatures;
        std::vector<TransactionInputDetails> inputs;
        std::vector<TransactionOutputDetails> outputs;
    };

    struct BlockDetails
    {
        uint8_t majorVersion = 0;
        uint8_t minorVersion = 0;
        uint64_t timestamp = 0;
        Crypto::Hash prevBlockHash;
        uint32_t nonce = 0;
        bool isAlternative = false;
        uint32_t index = 0;
        Crypto::Hash hash;
        uint64_t difficulty = 0;
        uint64_t reward = 0;
        uint64_t baseReward = 0;
        uint64_t blockSize = 0;
        uint64_t transactionsCumulativeSize = 0;
        uint64_t alreadyGeneratedCoins = 0;
        uint64_t alreadyGeneratedTransactions = 0;
        uint64_t sizeMedian = 0;
        double penalty = 0.0;
        uint64_t totalFeeAmount = 0;
        std::vector<TransactionDetails> transactions;
    };

} // namespace CryptoNote
