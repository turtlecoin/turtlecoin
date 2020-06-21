// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

/////////////////////////////////
#include <utilities/ParseExtra.h>
/////////////////////////////////

#include <common/Varint.h>
#include <config/Constants.h>

namespace Utilities
{
    std::string getPaymentIDFromExtra(const std::vector<uint8_t> &extra)
    {
        const ParsedExtra parsed = parseExtra(extra);
        return parsed.paymentID;
    }

    Crypto::PublicKey getTransactionPublicKeyFromExtra(const std::vector<uint8_t> &extra)
    {
        const ParsedExtra parsed = parseExtra(extra);
        return parsed.transactionPublicKey;
    }

    MergedMiningTag getMergedMiningTagFromExtra(const std::vector<uint8_t> &extra)
    {
        const ParsedExtra parsed = parseExtra(extra);
        return parsed.mergedMiningTag;
    }

    std::vector<uint8_t> getExtraDataFromExtra(const std::vector<uint8_t> &extra)
    {
        const ParsedExtra parsed = parseExtra(extra);
        return parsed.extraData;
    }

    ParsedExtra parseExtra(const std::vector<uint8_t> &extra)
    {
        ParsedExtra parsed {Constants::NULL_PUBLIC_KEY, std::string(), {0, Constants::NULL_HASH}};

        bool seenPubKey = false;
        bool seenNonce = false;
        bool seenExtraData = false;
        bool seenPaymentID = false;
        bool seenMergedMiningTag = false;
        bool seenRecipientPublicViewKey = false;
        bool seenRecipientPublicSpendKey = false;
        bool seenTransactionPrivateKey = false;

        for (auto it = extra.begin(); it < extra.end(); it++)
        {
            /* Nothing else to parse. */
            if (seenPubKey
             && seenPaymentID
             && seenMergedMiningTag
             && seenExtraData
             && seenRecipientPublicViewKey
             && seenRecipientPublicSpendKey
             && seenTransactionPrivateKey)
            {
                break;
            }

            const uint8_t c = *it;

            const auto elementsRemaining = std::distance(it, extra.end());

            /* Found a pubkey */

            /* Public key looks like this

               [...data...] 0x01 [public key] [...data...]

            */
            if (c == Constants::TX_EXTRA_PUBKEY_IDENTIFIER && elementsRemaining > 32 && !seenPubKey)
            {
                /* Copy 32 chars, beginning from the next char */
                std::copy(it + 1, it + 1 + 32, std::begin(parsed.transactionPublicKey.data));

                /* Advance past the public key */
                it += 32;

                seenPubKey = true;

                /* And continue parsing. */
                continue;
            }

            /* Found nonce information and need to decode it.
            /* Nonce is a sub-tagged field and thus we need to work through
               the data to determine what fields are in here */
            if (c == Constants::TX_EXTRA_NONCE_IDENTIFIER && elementsRemaining > 1 && !seenNonce)
            {
                /* Get the length of the following data in the field */
                size_t nonceSize = 0;

                const auto readNonceSize = Tools::read_varint(it + 1, extra.end(), nonceSize);

                /* Set up a variable to hold how much we have read so we know how far to skip ahead */
                size_t advanceIterator = readNonceSize;

                /* Only start reading if there are enough bytes left to read */
                if (elementsRemaining > readNonceSize + nonceSize)
                {
                    /* Copy the nonce data into a new array to make things easier to read through */
                    std::vector<uint8_t> nonceData;

                    std::copy(it + 1 + readNonceSize, it + 1 + readNonceSize + nonceSize, std::back_inserter(nonceData));

                    /* Loop through the nonce data looking for fields */
                    for (auto is = nonceData.begin(); is != nonceData.end(); is++)
                    {
                        const uint8_t s = *is;

                        const auto nElementsRemaining = std::distance(is, nonceData.end());

                        /* If we encounter a Payment ID field and there are enough bytes remaining in
                           the nonce data and we have not encountered a payment ID, then read it out */
                        if (s == Constants::TX_EXTRA_PAYMENT_ID_IDENTIFIER && nElementsRemaining > 32 && !seenPaymentID)
                        {
                            /* Grab the payment ID hash out of the nonce data */
                            Crypto::Hash paymentIDHash;

                            std::copy(is + 1, is + 1 + 32, std::begin(paymentIDHash.data));

                            /* Assign the payment ID to the parsed result */
                            parsed.paymentID = Common::podToHex(paymentIDHash);

                            seenPaymentID = true;

                            /* Advance the main iterator by the tag (1-byte) + the hash size (32-bytes) */
                            advanceIterator += 1 + 32;

                            /* Advance the inner iterator by the hash size (32-bytes) */
                            is += 32;

                            continue;
                        }

                        /* If we encounter an arbitrary data field and there is at least one byte remaining
                           and we have not encountered the arbitrary data yet, then read it out */
                        if (s == Constants::TX_EXTRA_ARBITRARY_DATA_IDENTIFIER && nElementsRemaining > 1 && !seenExtraData)
                        {
                            /* Read out the size of the data */
                            size_t dataSize = 0;

                            const auto readDataSize = Tools::read_varint(is + 1, nonceData.end(), dataSize);

                            /* If there are enough bytes left to read based upon the size above then
                               read out the data */
                            if (nElementsRemaining >= 1 + readDataSize + dataSize)
                            {
                                /* Copy the data into the parsed extraData field */
                                std::copy(is + 1 + readDataSize, is + 1 + readDataSize + dataSize, std::back_inserter(parsed.extraData));

                                seenExtraData = true;

                                /* Advance the main iterator by the tag (1-byte) + the length field (readDataSize)
                                   + the actual data size (dataSize) */
                                advanceIterator += 1 + readDataSize + dataSize;

                                /* Advance the inner iterator by the the length field (readDataSize)
                                   + the actual data size (dataSize) */
                                is += readDataSize + dataSize;

                                continue;
                            }
                        }
                    }
                }

                /* Advance the main iterator by the amount found in the nonce data */
                it += advanceIterator;

                seenNonce = true;

                /* And continue parsing */
                continue;
            }

            if (c == Constants::TX_EXTRA_MERGE_MINING_IDENTIFIER && elementsRemaining > 1 && !seenMergedMiningTag)
            {
                /* Get the length of the following data (Probably 33 bytes for depth+hash) */
                size_t dataSize = 0;

                const auto readDataSize = Tools::read_varint(it + 1, extra.end(), dataSize);

                if (elementsRemaining > dataSize + readDataSize && dataSize >= 33)
                {
                    size_t depth = 0;

                    const auto readDepthSize = Tools::read_varint(it + 1 + readDataSize, extra.end(), depth);

                    Crypto::Hash merkleRoot;

                    const auto dataBegin = it + 1 + readDataSize + readDepthSize;

                    std::copy(dataBegin, dataBegin + 32, std::begin(merkleRoot.data));

                    parsed.mergedMiningTag.depth = depth;
                    parsed.mergedMiningTag.merkleRoot = merkleRoot;

                    /* Advance past the mm tag by length field (readDataSize) + */
                    it += readDepthSize + 32;

                    seenMergedMiningTag = true;

                    /* Can continue parsing */
                    continue;
                }
            }

            if (c == Constants::TX_EXTRA_RECIPIENT_PUBLIC_VIEW_KEY_IDENTIFIER && elementsRemaining > 32 && !seenRecipientPublicViewKey)
            {
                /* Copy 32 chars, beginning from the next char */
                std::copy(it + 1, it + 1 + 32, std::begin(parsed.recipientPublicViewKey.data));

                /* Advance past the public view key */
                it += 32;

                seenRecipientPublicViewKey = true;

                /* And continue parsing. */
                continue;
            }

            if (c == Constants::TX_EXTRA_RECIPIENT_PUBLIC_SPEND_KEY_IDENTIFIER && elementsRemaining > 32 && !seenRecipientPublicSpendKey)
            {
                /* Copy 32 chars, beginning from the next char */
                std::copy(it + 1, it + 1 + 32, std::begin(parsed.recipientPublicSpendKey.data));

                /* Advance past the public spend key */
                it += 32;

                seenRecipientPublicSpendKey = true;

                /* And continue parsing. */
                continue;
            }

            if (c == Constants::TX_EXTRA_TRANSACTION_PRIVATE_KEY_IDENTIFIER && elementsRemaining > 32 && !seenTransactionPrivateKey)
            {
                /* Copy 32 chars, beginning from the next char */
                std::copy(it + 1, it + 1 + 32, std::begin(parsed.transactionPrivateKey.data));

                /* Advance past the private key */
                it += 32;

                seenTransactionPrivateKey = true;

                /* And continue parsing. */
                continue;
            }
        }

        return parsed;
    }
} // namespace Utilities
