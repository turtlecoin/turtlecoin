// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoNote.h>
#include <algorithm>
#include <boost/variant.hpp>
#include <vector>

#define TX_EXTRA_PADDING_MAX_COUNT 255
#define TX_EXTRA_NONCE_MAX_COUNT 255

#define TX_EXTRA_TAG_PADDING 0x00
#define TX_EXTRA_TAG_PUBKEY 0x01
#define TX_EXTRA_NONCE 0x02
#define TX_EXTRA_MERGE_MINING_TAG 0x03
#define TX_EXTRA_POOL 0x07

#define TX_EXTRA_NONCE_PAYMENT_ID 0x00

namespace CryptoNote
{
    struct TransactionExtraPadding
    {
        size_t size;
    };

    struct TransactionExtraPublicKey
    {
        Crypto::PublicKey publicKey;
    };

    struct TransactionExtraNonce
    {
        std::vector<uint8_t> nonce;
    };

    struct TransactionExtraMergeMiningTag
    {
        size_t depth;
        Crypto::Hash merkleRoot;
    };

    // tx_extra_field format, except tx_extra_padding and tx_extra_pub_key:
    //   varint tag;
    //   varint size;
    //   varint data[];
    typedef boost::variant<
        TransactionExtraPadding,
        TransactionExtraPublicKey,
        TransactionExtraNonce,
        TransactionExtraMergeMiningTag>
        TransactionExtraField;

    template<typename T>
    bool findTransactionExtraFieldByType(const std::vector<TransactionExtraField> &tx_extra_fields, T &field)
    {
        auto it = std::find_if(tx_extra_fields.begin(), tx_extra_fields.end(), [](const TransactionExtraField &f) {
            return typeid(T) == f.type();
        });

        if (tx_extra_fields.end() == it)
        {
            return false;
        }

        field = boost::get<T>(*it);
        return true;
    }

    bool parseTransactionExtra(
        const std::vector<uint8_t> &tx_extra,
        std::vector<TransactionExtraField> &tx_extra_fields);

    bool writeTransactionExtra(
        std::vector<uint8_t> &tx_extra,
        const std::vector<TransactionExtraField> &tx_extra_fields);

    Crypto::PublicKey getTransactionPublicKeyFromExtra(const std::vector<uint8_t> &tx_extra);

    bool addTransactionPublicKeyToExtra(std::vector<uint8_t> &tx_extra, const Crypto::PublicKey &tx_pub_key);

    bool addExtraNonceToTransactionExtra(std::vector<uint8_t> &tx_extra, const BinaryArray &extra_nonce);

    bool addPoolNonceToTransactionExtra(std::vector<uint8_t> &tx_extra, const BinaryArray &extra_nonce);

    void setPaymentIdToTransactionExtraNonce(BinaryArray &extra_nonce, const Crypto::Hash &payment_id);

    bool getPaymentIdFromTransactionExtraNonce(const BinaryArray &extra_nonce, Crypto::Hash &payment_id);

    bool appendMergeMiningTagToExtra(std::vector<uint8_t> &tx_extra, const TransactionExtraMergeMiningTag &mm_tag);

    bool getMergeMiningTagFromExtra(const std::vector<uint8_t> &tx_extra, TransactionExtraMergeMiningTag &mm_tag);

    bool createTxExtraWithPaymentId(const std::string &paymentIdString, std::vector<uint8_t> &extra);

    // returns false if payment id is not found or parse error
    bool getPaymentIdFromTxExtra(const std::vector<uint8_t> &extra, Crypto::Hash &paymentId);

    bool parsePaymentId(const std::string &paymentIdString, Crypto::Hash &paymentId);

} // namespace CryptoNote
