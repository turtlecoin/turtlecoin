// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoTypes.h>
#include <string>
#include <vector>

namespace Utilities
{
    struct MergedMiningTag
    {
        uint8_t depth;
        Crypto::Hash merkleRoot;
    };

    struct ParsedExtra
    {
        Crypto::PublicKey transactionPublicKey;
        std::string paymentID;
        MergedMiningTag mergedMiningTag;
        std::vector<uint8_t> extraData;

        /* Coinbase transaction only */
        Crypto::PublicKey recipientPublicViewKey;
        Crypto::PublicKey recipientPublicSpendKey;
        Crypto::SecretKey transactionPrivateKey;
    };

    std::string getPaymentIDFromExtra(const std::vector<uint8_t> &extra);

    Crypto::PublicKey getTransactionPublicKeyFromExtra(const std::vector<uint8_t> &extra);

    MergedMiningTag getMergedMiningTagFromExtra(const std::vector<uint8_t> &extra);

    std::vector<uint8_t> getExtraDataFromExtra(const std::vector<uint8_t> &extra);

    ParsedExtra parseExtra(const std::vector<uint8_t> &extra);
} // namespace Utilities
