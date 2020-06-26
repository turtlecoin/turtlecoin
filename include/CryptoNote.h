// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoTypes.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"

#include <JsonHelper.h>
#include <boost/variant.hpp>
#include <common/StringTools.h>
#include <vector>

namespace CryptoNote
{
    struct BaseInput
    {
        uint32_t blockIndex;
    };

    struct KeyInput
    {
        uint64_t amount;

        std::vector<uint32_t> outputIndexes;

        Crypto::KeyImage keyImage;

        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.StartObject();
            {
                writer.Key("amount");
                writer.Uint64(amount);

                writer.Key("key_offsets");
                writer.StartArray();
                {
                    for (const auto &offset : outputIndexes)
                    {
                        writer.Uint(offset);
                    }
                }
                writer.EndArray();

                writer.Key("k_image");
                keyImage.toJSON(writer);
            }
            writer.EndObject();
        }

        void fromJSON(const JSONValue &j)
        {
            amount = getUint64FromJSON(j, "amount");

            outputIndexes.clear();

            for (const auto &offset : getArrayFromJSON(j, "key_offsets"))
            {
                uint32_t index = getUintFromJSON(offset);

                outputIndexes.push_back(index);
            }

            keyImage.fromString(getStringFromJSON(j, "k_image"));
        }
    };

    struct KeyOutput
    {
        Crypto::PublicKey key;
    };

    typedef boost::variant<BaseInput, KeyInput> TransactionInput;

    typedef boost::variant<KeyOutput> TransactionOutputTarget;

    struct TransactionOutput
    {
        uint64_t amount;

        TransactionOutputTarget target;
    };

    struct TransactionPrefix
    {
        uint8_t version;

        uint64_t unlockTime;

        std::vector<TransactionInput> inputs;

        std::vector<TransactionOutput> outputs;

        std::vector<uint8_t> extra;
    };

    struct Transaction : public TransactionPrefix
    {
        std::vector<std::vector<Crypto::Signature>> signatures;
    };

    struct BaseTransaction : public TransactionPrefix
    {
    };

    struct ParentBlock
    {
        uint8_t majorVersion;

        uint8_t minorVersion;

        Crypto::Hash previousBlockHash;

        uint16_t transactionCount;

        std::vector<Crypto::Hash> baseTransactionBranch;

        BaseTransaction baseTransaction;

        std::vector<Crypto::Hash> blockchainBranch;
    };

    struct BlockHeader
    {
        uint8_t majorVersion;

        uint8_t minorVersion;

        uint32_t nonce;

        uint64_t timestamp;

        Crypto::Hash previousBlockHash;
    };

    struct BlockTemplate : public BlockHeader
    {
        ParentBlock parentBlock;

        Transaction baseTransaction;

        std::vector<Crypto::Hash> transactionHashes;
    };

    struct AccountPublicAddress
    {
        Crypto::PublicKey spendPublicKey;

        Crypto::PublicKey viewPublicKey;
    };

    struct AccountKeys
    {
        AccountPublicAddress address;

        Crypto::SecretKey spendSecretKey;

        Crypto::SecretKey viewSecretKey;
    };

    struct KeyPair
    {
        Crypto::PublicKey publicKey;

        Crypto::SecretKey secretKey;
    };

    using BinaryArray = std::vector<uint8_t>;

    struct RawBlock
    {
        BinaryArray block; // BlockTemplate

        std::vector<BinaryArray> transactions;

        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.StartObject();
            {
                writer.Key("blob");
                writer.String(Common::toHex(block));

                writer.Key("transactions");
                writer.StartArray();
                {
                    for (const auto &transaction : transactions)
                    {
                        writer.String(Common::toHex(transaction));
                    }
                }
                writer.EndArray();
            }
            writer.EndObject();
        }

        void fromJSON(const JSONValue &j)
        {
            block = Common::fromHex(getStringFromJSON(j, "blob"));

            for (const auto &tx : getArrayFromJSON(j, "transactions"))
            {
                transactions.push_back(Common::fromHex(tx.GetString()));
            }
        }
    };
} // namespace CryptoNote
