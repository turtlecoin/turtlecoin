// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "TransactionExtra.h"

#include "common/MemoryInputStream.h"
#include "common/StreamTools.h"
#include "common/StringTools.h"
#include "serialization/BinaryInputStreamSerializer.h"
#include "serialization/BinaryOutputStreamSerializer.h"
#include "serialization/SerializationTools.h"

using namespace Crypto;
using namespace Common;

namespace CryptoNote
{
    bool parseTransactionExtra(
        const std::vector<uint8_t> &transactionExtra,
        std::vector<TransactionExtraField> &transactionExtraFields)
    {
        transactionExtraFields.clear();

        if (transactionExtra.empty())
        {
            return true;
        }

        bool seen_tx_extra_tag_padding = false;
        bool seen_tx_extra_tag_pubkey = false;
        bool seen_tx_extra_nonce = false;
        bool seen_tx_extra_merge_mining_tag = false;

        try
        {
            MemoryInputStream iss(transactionExtra.data(), transactionExtra.size());
            BinaryInputStreamSerializer ar(iss);

            int c = 0;

            while (!iss.endOfStream())
            {
                c = read<uint8_t>(iss);
                switch (c)
                {
                    case TX_EXTRA_TAG_PADDING:
                    {
                        if (seen_tx_extra_tag_padding)
                        {
                            return true;
                        }

                        seen_tx_extra_tag_padding = true;

                        size_t size = 1;
                        for (; !iss.endOfStream() && size <= TX_EXTRA_PADDING_MAX_COUNT; ++size)
                        {
                            if (read<uint8_t>(iss) != 0)
                            {
                                return false; // all bytes should be zero
                            }
                        }

                        if (size > TX_EXTRA_PADDING_MAX_COUNT)
                        {
                            return false;
                        }

                        transactionExtraFields.push_back(TransactionExtraPadding {size});
                        break;
                    }

                    case TX_EXTRA_TAG_PUBKEY:
                    {
                        if (seen_tx_extra_tag_pubkey)
                        {
                            return true;
                        }

                        seen_tx_extra_tag_pubkey = true;

                        TransactionExtraPublicKey extraPk;
                        ar(extraPk.publicKey, "public_key");
                        transactionExtraFields.push_back(extraPk);
                        break;
                    }

                    case TX_EXTRA_NONCE:
                    {
                        if (seen_tx_extra_nonce)
                        {
                            return true;
                        }

                        seen_tx_extra_nonce = true;

                        TransactionExtraNonce extraNonce;
                        uint8_t size = read<uint8_t>(iss);
                        if (size > 0)
                        {
                            extraNonce.nonce.resize(size);
                            read(iss, extraNonce.nonce.data(), extraNonce.nonce.size());
                        }

                        transactionExtraFields.push_back(extraNonce);
                        break;
                    }

                    case TX_EXTRA_MERGE_MINING_TAG:
                    {
                        if (seen_tx_extra_merge_mining_tag)
                        {
                            break;
                        }

                        seen_tx_extra_merge_mining_tag = true;

                        TransactionExtraMergeMiningTag mmTag;
                        ar(mmTag, "mm_tag");
                        transactionExtraFields.push_back(mmTag);
                        break;
                    }
                }
            }
        }
        catch (std::exception &)
        {
            return false;
        }

        return true;
    }

    struct ExtraSerializerVisitor : public boost::static_visitor<bool>
    {
        std::vector<uint8_t> &extra;

        ExtraSerializerVisitor(std::vector<uint8_t> &tx_extra): extra(tx_extra) {}

        bool operator()(const TransactionExtraPadding &t)
        {
            if (t.size > TX_EXTRA_PADDING_MAX_COUNT)
            {
                return false;
            }
            extra.insert(extra.end(), t.size, 0);
            return true;
        }

        bool operator()(const TransactionExtraPublicKey &t)
        {
            return addTransactionPublicKeyToExtra(extra, t.publicKey);
        }

        bool operator()(const TransactionExtraNonce &t)
        {
            return addExtraNonceToTransactionExtra(extra, t.nonce);
        }

        bool operator()(const TransactionExtraMergeMiningTag &t)
        {
            return appendMergeMiningTagToExtra(extra, t);
        }
    };

    bool
        writeTransactionExtra(std::vector<uint8_t> &tx_extra, const std::vector<TransactionExtraField> &tx_extra_fields)
    {
        ExtraSerializerVisitor visitor(tx_extra);

        for (const auto &tag : tx_extra_fields)
        {
            if (!boost::apply_visitor(visitor, tag))
            {
                return false;
            }
        }

        return true;
    }

    PublicKey getTransactionPublicKeyFromExtra(const std::vector<uint8_t> &tx_extra)
    {
        std::vector<TransactionExtraField> tx_extra_fields;
        parseTransactionExtra(tx_extra, tx_extra_fields);

        TransactionExtraPublicKey pub_key_field;
        if (!findTransactionExtraFieldByType(tx_extra_fields, pub_key_field))
        {
            return Crypto::PublicKey();
        }

        return pub_key_field.publicKey;
    }

    bool addTransactionPublicKeyToExtra(std::vector<uint8_t> &tx_extra, const PublicKey &tx_pub_key)
    {
        tx_extra.resize(tx_extra.size() + 1 + sizeof(PublicKey));
        tx_extra[tx_extra.size() - 1 - sizeof(PublicKey)] = TX_EXTRA_TAG_PUBKEY;
        *reinterpret_cast<PublicKey *>(&tx_extra[tx_extra.size() - sizeof(PublicKey)]) = tx_pub_key;
        return true;
    }

    bool addExtraNonceToTransactionExtra(std::vector<uint8_t> &tx_extra, const BinaryArray &extra_nonce)
    {
        if (extra_nonce.size() > TX_EXTRA_NONCE_MAX_COUNT)
        {
            return false;
        }

        size_t start_pos = tx_extra.size();
        tx_extra.resize(tx_extra.size() + 2 + extra_nonce.size());
        // write tag
        tx_extra[start_pos] = TX_EXTRA_NONCE;
        // write len
        ++start_pos;
        tx_extra[start_pos] = static_cast<uint8_t>(extra_nonce.size());
        // write data
        ++start_pos;
        memcpy(&tx_extra[start_pos], extra_nonce.data(), extra_nonce.size());
        return true;
    }

    bool appendMergeMiningTagToExtra(std::vector<uint8_t> &tx_extra, const TransactionExtraMergeMiningTag &mm_tag)
    {
        BinaryArray blob;
        if (!toBinaryArray(mm_tag, blob))
        {
            return false;
        }

        tx_extra.push_back(TX_EXTRA_MERGE_MINING_TAG);
        std::copy(
            reinterpret_cast<const uint8_t *>(blob.data()),
            reinterpret_cast<const uint8_t *>(blob.data() + blob.size()),
            std::back_inserter(tx_extra));
        return true;
    }

    bool getMergeMiningTagFromExtra(const std::vector<uint8_t> &tx_extra, TransactionExtraMergeMiningTag &mm_tag)
    {
        std::vector<TransactionExtraField> tx_extra_fields;
        parseTransactionExtra(tx_extra, tx_extra_fields);

        return findTransactionExtraFieldByType(tx_extra_fields, mm_tag);
    }

    void setPaymentIdToTransactionExtraNonce(std::vector<uint8_t> &extra_nonce, const Hash &payment_id)
    {
        extra_nonce.clear();
        extra_nonce.push_back(TX_EXTRA_NONCE_PAYMENT_ID);
        const uint8_t *payment_id_ptr = reinterpret_cast<const uint8_t *>(&payment_id);
        std::copy(payment_id_ptr, payment_id_ptr + sizeof(payment_id), std::back_inserter(extra_nonce));
    }

    bool getPaymentIdFromTransactionExtraNonce(const std::vector<uint8_t> &extra_nonce, Hash &payment_id)
    {
        if (sizeof(Hash) + 1 != extra_nonce.size())
        {
            return false;
        }
        if (TX_EXTRA_NONCE_PAYMENT_ID != extra_nonce[0])
        {
            return false;
        }
        payment_id = *reinterpret_cast<const Hash *>(extra_nonce.data() + 1);
        return true;
    }

    bool parsePaymentId(const std::string &paymentIdString, Hash &paymentId)
    {
        return Common::podFromHex(paymentIdString, paymentId);
    }

    bool createTxExtraWithPaymentId(const std::string &paymentIdString, std::vector<uint8_t> &extra)
    {
        Hash paymentIdBin;

        if (!parsePaymentId(paymentIdString, paymentIdBin))
        {
            return false;
        }

        std::vector<uint8_t> extraNonce;
        CryptoNote::setPaymentIdToTransactionExtraNonce(extraNonce, paymentIdBin);

        if (!CryptoNote::addExtraNonceToTransactionExtra(extra, extraNonce))
        {
            return false;
        }

        return true;
    }

    bool getPaymentIdFromTxExtra(const std::vector<uint8_t> &extra, Hash &paymentId)
    {
        std::vector<TransactionExtraField> tx_extra_fields;
        if (!parseTransactionExtra(extra, tx_extra_fields))
        {
            return false;
        }

        TransactionExtraNonce extra_nonce;
        if (findTransactionExtraFieldByType(tx_extra_fields, extra_nonce))
        {
            if (!getPaymentIdFromTransactionExtraNonce(extra_nonce.nonce, paymentId))
            {
                return false;
            }
        }
        else
        {
            return false;
        }

        return true;
    }

} // namespace CryptoNote
