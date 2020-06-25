// Copyright (c) 2018-2020, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "rapidjson/document.h"
#include "rapidjson/writer.h"

#include <CryptoNote.h>
#include <errors/Errors.h>
#include <JsonHelper.h>
#include <numeric>
#include <optional>
#include <string>
#include <unordered_map>

namespace WalletTypes
{
    struct KeyOutput
    {
        Crypto::PublicKey key;

        uint64_t amount;

        std::optional<uint64_t> globalOutputIndex;

        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.StartObject();
            {
                writer.Key("key");
                key.toJSON(writer);

                writer.Key("amount");
                writer.Uint64(amount);
            }
            writer.EndObject();
        }

        void fromJSON(const JSONValue &j)
        {
            key.fromString(getStringFromJSON(j, "key"));

            amount = getUint64FromJSON(j, "amount");

            if (hasMember(j, "globalIndex"))
            {
                globalOutputIndex = getUint64FromJSON(j, "globalIndex");
            }
        }
    };

    /* A coinbase transaction (i.e., a miner reward, there is one of these in
       every block). Coinbase transactions have no inputs. We call this a raw
       transaction, because it is simply key images and amounts */
    struct RawCoinbaseTransaction
    {
        /* The outputs of the transaction, amounts and keys */
        std::vector<KeyOutput> keyOutputs;

        /* The hash of the transaction */
        Crypto::Hash hash;

        /* The public key of this transaction, taken from the tx extra */
        Crypto::PublicKey transactionPublicKey;

        /* When this transaction's inputs become spendable. Some genius thought
           it was a good idea to use this field as both a block height, and a
           unix timestamp. If the value is greater than
           CRYPTONOTE_MAX_BLOCK_NUMBER (In cryptonoteconfig) it is treated
           as a unix timestamp, else it is treated as a block height. */
        uint64_t unlockTime;

        size_t memoryUsage() const
        {
            return keyOutputs.size() * sizeof(KeyOutput) + sizeof(keyOutputs) + sizeof(hash)
                   + sizeof(transactionPublicKey) + sizeof(unlockTime);
        }

        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.StartObject();
            {
                writer.Key("outputs");
                writer.StartArray();
                {
                    for (const auto &output : keyOutputs)
                    {
                        output.toJSON(writer);
                    }
                }
                writer.EndArray();

                writer.Key("hash");
                hash.toJSON(writer);

                writer.Key("txPublicKey");
                transactionPublicKey.toJSON(writer);

                writer.Key("unlockTime");
                writer.Uint64(unlockTime);
            }
            writer.EndObject();
        }

        void fromJSON(const JSONValue &j) {
            keyOutputs.clear();

            for (const auto &output : getArrayFromJSON(j, "outputs"))
            {
                KeyOutput key;

                key.fromJSON(output);

                keyOutputs.push_back(key);
            }

            hash.fromString(getStringFromJSON(j, "hash"));

            transactionPublicKey.fromString(getStringFromJSON(j, "txPublicKey"));

            unlockTime = getUint64FromJSON(j, "unlockTime");
        }
    };

    /* A raw transaction, simply key images and amounts */
    struct RawTransaction : RawCoinbaseTransaction
    {
        /* The transaction payment ID - may be an empty string */
        std::string paymentID;

        /* The inputs used for a transaction, can be used to track outgoing
           transactions */
        std::vector<CryptoNote::KeyInput> keyInputs;

        size_t memoryUsage() const
        {
            return paymentID.size() * sizeof(char) + sizeof(paymentID) + keyInputs.size() * sizeof(CryptoNote::KeyInput)
                   + sizeof(keyInputs) + RawCoinbaseTransaction::memoryUsage();
        }

        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.StartObject();
            {
                writer.Key("outputs");
                writer.StartArray();
                {
                    for (const auto &output : keyOutputs)
                    {
                        output.toJSON(writer);
                    }
                }
                writer.EndArray();

                writer.Key("hash");
                hash.toJSON(writer);

                writer.Key("txPublicKey");
                transactionPublicKey.toJSON(writer);

                writer.Key("unlockTime");
                writer.Uint64(unlockTime);

                writer.Key("paymentID");
                writer.String(paymentID);

                writer.Key("inputs");
                writer.StartArray();
                {
                    for (const auto &input : keyInputs)
                    {
                        input.toJSON(writer);
                    }
                }
                writer.EndArray();
            }
            writer.EndObject();
        }

        void fromJSON(const JSONValue &j) {
            keyOutputs.clear();

            for (const auto &output : getArrayFromJSON(j, "outputs"))
            {
                KeyOutput key;

                key.fromJSON(output);

                keyOutputs.push_back(key);
            }

            hash.fromString(getStringFromJSON(j, "hash"));

            transactionPublicKey.fromString(getStringFromJSON(j, "txPublicKey"));

            unlockTime = getUint64FromJSON(j, "unlockTime");

            paymentID = getStringFromJSON(j, "paymentID");

            keyInputs.clear();

            for (const auto &input : getArrayFromJSON(j, "inputs"))
            {
                CryptoNote::KeyInput key;

                key.fromJSON(input);

                keyInputs.push_back(key);
            }
        }
    };

    /* A 'block' with the very basics needed to sync the transactions */
    struct WalletBlockInfo
    {
        /* The coinbase transaction. Optional, since we can skip fetching
           coinbase transactions from daemon. */
        std::optional<RawCoinbaseTransaction> coinbaseTransaction;

        /* The transactions in the block */
        std::vector<RawTransaction> transactions;

        /* The block height (duh!) */
        uint64_t blockHeight;

        /* The hash of the block */
        Crypto::Hash blockHash;

        /* The timestamp of the block */
        uint64_t blockTimestamp;

        size_t memoryUsage() const
        {
            const size_t txUsage = std::accumulate(
                transactions.begin(), transactions.end(), sizeof(transactions), [](const auto acc, const auto item) {
                    return acc + item.memoryUsage();
                });

            return coinbaseTransaction ? coinbaseTransaction->memoryUsage()
                                       : sizeof(coinbaseTransaction) + txUsage + sizeof(blockHeight) + sizeof(blockHash)
                                             + sizeof(blockTimestamp);
        }

        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.StartObject();
            {
                writer.Key("transactions");
                writer.StartArray();
                {
                    for (const auto &transaction : transactions)
                    {
                        transaction.toJSON(writer);
                    }
                }
                writer.EndArray();

                writer.Key("blockHeight");
                writer.Uint64(blockHeight);

                writer.Key("blockHash");
                blockHash.toJSON(writer);

                writer.Key("blockTimestamp");
                writer.Uint64(blockTimestamp);

                if (coinbaseTransaction)
                {
                    const auto coinbase = coinbaseTransaction.value();

                    writer.Key("coinbaseTX");
                    coinbase.toJSON(writer);
                }
            }
            writer.EndObject();
        }

        void fromJSON(const JSONValue &j)
        {
            transactions.clear();

            for (const auto &transaction : getArrayFromJSON(j, "transactions"))
            {
                RawTransaction tx;

                tx.fromJSON(transaction);

                transactions.push_back(tx);
            }

            blockHeight = getUint64FromJSON(j, "blockHeight");

            blockHash.fromString(getStringFromJSON(j, "blockHash"));

            blockTimestamp = getUint64FromJSON(j, "blockTimestamp");

            if (hasMember(j, "coinbaseTX"))
            {
                RawCoinbaseTransaction tx;

                tx.fromJSON(getJsonValue(j, "coinbaseTX"));

                coinbaseTransaction = tx;
            }
        }
    };

    struct TransactionInput
    {
        /* The key image of this amount */
        Crypto::KeyImage keyImage;

        /* The value of this key image */
        uint64_t amount;

        /* The block height this key images transaction was included in
           (Need this for removing key images that were received on a forked
           chain) */
        uint64_t blockHeight;

        /* The transaction public key that was included in the tx_extra of the
           transaction */
        Crypto::PublicKey transactionPublicKey;

        /* The index of this input in the transaction */
        uint64_t transactionIndex;

        /* The index of this output in the 'DB' */
        std::optional<uint64_t> globalOutputIndex;

        /* The transaction key we took from the key outputs */
        Crypto::PublicKey key;

        /* If spent, what height did we spend it at. Used to remove spent
           transaction inputs once they are sure to not be removed from a
           forked chain. */
        uint64_t spendHeight;

        /* When does this input unlock for spending. Default is instantly
           unlocked, or blockHeight + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW
           for a coinbase/miner transaction. Users can specify a custom
           unlock height however. */
        uint64_t unlockTime;

        /* The transaction hash of the transaction that contains this input */
        Crypto::Hash parentTransactionHash;

        /* The private ephemeral generated along with the key image */
        std::optional<Crypto::SecretKey> privateEphemeral;

        bool operator==(const TransactionInput &other)
        {
            return keyImage == other.keyImage;
        }

        /* Converts the class to a json object */
        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.StartObject();
            {
                writer.Key("keyImage");
                keyImage.toJSON(writer);

                writer.Key("amount");
                writer.Uint64(amount);

                writer.Key("blockHeight");
                writer.Uint64(blockHeight);

                writer.Key("transactionPublicKey");
                transactionPublicKey.toJSON(writer);

                writer.Key("transactionIndex");
                writer.Uint64(transactionIndex);

                writer.Key("globalOutputIndex");
                writer.Uint64(globalOutputIndex.value_or(0));

                writer.Key("key");
                key.toJSON(writer);

                writer.Key("spendHeight");
                writer.Uint64(spendHeight);

                writer.Key("unlockTime");
                writer.Uint64(unlockTime);

                writer.Key("parentTransactionHash");
                parentTransactionHash.toJSON(writer);

                if (privateEphemeral)
                {
                    writer.Key("privateEphemeral");
                    privateEphemeral->toJSON(writer);
                }
            }
            writer.EndObject();
        }

        /* Initializes the class from a json string */
        void fromJSON(const JSONValue &j)
        {
            keyImage.fromString(getStringFromJSON(j, "keyImage"));

            amount = getUint64FromJSON(j, "amount");

            blockHeight = getUint64FromJSON(j, "blockHeight");

            transactionPublicKey.fromString(getStringFromJSON(j, "transactionPublicKey"));

            transactionIndex = getUint64FromJSON(j, "transactionIndex");

            globalOutputIndex = getUint64FromJSON(j, "globalOutputIndex");

            key.fromString(getStringFromJSON(j, "key"));

            spendHeight = getUint64FromJSON(j, "spendHeight");

            unlockTime = getUint64FromJSON(j, "unlockTime");

            parentTransactionHash.fromString(getStringFromJSON(j, "parentTransactionHash"));

            if (j.HasMember("privateEphemeral"))
            {
                Crypto::SecretKey tmp;

                tmp.fromString(getStringFromJSON(j, "privateEphemeral"));

                privateEphemeral = tmp;
            }
        }
    };

    /* Includes the owner of the input so we can sign the input with the
       correct keys */
    struct TxInputAndOwner
    {
        TxInputAndOwner(
            const TransactionInput input,
            const Crypto::PublicKey publicSpendKey,
            const Crypto::SecretKey privateSpendKey):
            input(input),
            publicSpendKey(publicSpendKey),
            privateSpendKey(privateSpendKey)
        {
        }

        TransactionInput input;

        Crypto::PublicKey publicSpendKey;

        Crypto::SecretKey privateSpendKey;
    };

    struct TransactionDestination
    {
        /* The public spend key of the receiver of the transaction output */
        Crypto::PublicKey receiverPublicSpendKey;

        /* The public view key of the receiver of the transaction output */
        Crypto::PublicKey receiverPublicViewKey;

        /* The amount of the transaction output */
        uint64_t amount;
    };

    struct GlobalIndexKey
    {
        uint64_t index;

        Crypto::PublicKey key;
    };

    struct ObscuredInput
    {
        /* The outputs, including our real output, and the fake mixin outputs */
        std::vector<GlobalIndexKey> outputs;

        /* The index of the real output in the outputs vector */
        uint64_t realOutput;

        /* The real transaction public key */
        Crypto::PublicKey realTransactionPublicKey;

        /* The index in the transaction outputs vector */
        uint64_t realOutputTransactionIndex;

        /* The amount being sent */
        uint64_t amount;

        /* The owners keys, so we can sign the input correctly */
        Crypto::PublicKey ownerPublicSpendKey;

        Crypto::SecretKey ownerPrivateSpendKey;

        /* The key image of the input */
        Crypto::KeyImage keyImage;

        /* The private ephemeral generated along with the key image */
        std::optional<Crypto::SecretKey> privateEphemeral;
    };

    class Transaction
    {
      public:
        //////////////////
        /* Constructors */
        //////////////////

        Transaction() {};

        Transaction(
            /* Mapping of public key to transaction amount, can be multiple
               if one transaction sends to multiple subwallets */
            const std::unordered_map<Crypto::PublicKey, int64_t> transfers,
            const Crypto::Hash hash,
            const uint64_t fee,
            const uint64_t timestamp,
            const uint64_t blockHeight,
            const std::string paymentID,
            const uint64_t unlockTime,
            const bool isCoinbaseTransaction):
            transfers(transfers),
            hash(hash),
            fee(fee),
            timestamp(timestamp),
            blockHeight(blockHeight),
            paymentID(paymentID),
            unlockTime(unlockTime),
            isCoinbaseTransaction(isCoinbaseTransaction)
        {
        }

        /////////////////////////////
        /* Public member functions */
        /////////////////////////////

        int64_t totalAmount() const
        {
            int64_t sum = 0;

            for (const auto [pubKey, amount] : transfers)
            {
                sum += amount;
            }

            return sum;
        }

        /* It's worth noting that this isn't a conclusive check for if a
           transaction is a fusion transaction - there are some requirements
           it has to meet - but we don't need to check them, as the daemon
           will handle that for us - Any transactions that come to the
           wallet (assuming a non malicious daemon) that are zero and not
           a coinbase, is a fusion transaction */
        bool isFusionTransaction() const
        {
            return fee == 0 && !isCoinbaseTransaction;
        }

        /////////////////////////////
        /* Public member variables */
        /////////////////////////////

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

        /* The blockheight this transaction is in */
        uint64_t blockHeight;

        /* The timestamp of this transaction (taken from the block timestamp) */
        uint64_t timestamp;

        /* The paymentID of this transaction (will be an empty string if no pid) */
        std::string paymentID;

        /* When does the transaction unlock */
        uint64_t unlockTime;

        /* Was this transaction a miner reward / coinbase transaction */
        bool isCoinbaseTransaction;

        /* Converts the class to a json object */
        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.StartObject();
            {
                writer.Key("transfers");
                writer.StartArray();
                {
                    for (const auto &[publicKey, amount] : transfers)
                    {
                        writer.StartObject();
                        {
                            writer.Key("publicKey");
                            publicKey.toJSON(writer);

                            writer.Key("amount");
                            writer.Int64(amount);
                        }
                        writer.EndObject();
                    }
                }
                writer.EndArray();

                writer.Key("hash");
                hash.toJSON(writer);

                writer.Key("fee");
                writer.Uint64(fee);

                writer.Key("blockHeight");
                writer.Uint64(blockHeight);

                writer.Key("timestamp");
                writer.Uint64(timestamp);

                writer.Key("paymentID");
                writer.String(paymentID);

                writer.Key("unlockTime");
                writer.Uint64(unlockTime);

                writer.Key("isCoinbaseTransaction");
                writer.Bool(isCoinbaseTransaction);
            }
            writer.EndObject();
        }

        /* Initializes the class from a json string */
        void fromJSON(const JSONValue &j)
        {
            for (const auto &x : getArrayFromJSON(j, "transfers"))
            {
                Crypto::PublicKey publicKey;

                publicKey.fromString(getStringFromJSON(x, "publicKey"));

                transfers[publicKey] = getInt64FromJSON(x, "amount");
            }

            hash.fromString(getStringFromJSON(j, "hash"));

            fee = getUint64FromJSON(j, "fee");

            blockHeight = getUint64FromJSON(j, "blockHeight");

            timestamp = getUint64FromJSON(j, "timestamp");

            paymentID = getStringFromJSON(j, "paymentID");

            unlockTime = getUint64FromJSON(j, "unlockTime");

            isCoinbaseTransaction = getBoolFromJSON(j, "isCoinbaseTransaction");
        }
    };

    struct WalletStatus
    {
        /* The amount of blocks the wallet has synced */
        uint64_t walletBlockCount;

        /* The amount of blocks the daemon we are connected to has synced */
        uint64_t localDaemonBlockCount;

        /* The amount of blocks the daemons on the network have */
        uint64_t networkBlockCount;

        /* The amount of peers the node is connected to */
        uint32_t peerCount;

        /* The hashrate (based on the last block the daemon has synced) */
        uint64_t lastKnownHashrate;
    };

    /* A structure just used to display locked balance, due to change from
       sent transactions. We just need the amount and a unique identifier
       (hash+key), since we can't spend it, we don't need all the other stuff */
    struct UnconfirmedInput
    {
        /* The amount of the input */
        uint64_t amount;

        /* The transaction key we took from the key outputs */
        Crypto::PublicKey key;

        /* The transaction hash of the transaction that contains this input */
        Crypto::Hash parentTransactionHash;

        /* Converts the class to a json object */
        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.StartObject();
            {
                writer.Key("amount");
                writer.Uint64(amount);

                writer.Key("key");
                key.toJSON(writer);

                writer.Key("parentTransactionHash");
                parentTransactionHash.toJSON(writer);
            }
            writer.EndObject();
        }

        /* Initializes the class from a json string */
        void fromJSON(const JSONValue &j)
        {
            amount = getUint64FromJSON(j, "amount");

            key.fromString(getStringFromJSON(j, "key"));

            parentTransactionHash.fromString(getStringFromJSON(j, "parentTransactionHash"));
        }
    };

    struct TopBlock
    {
        Crypto::Hash hash;

        uint64_t height;

        void fromJSON(const JSONValue &j)
        {
            hash.fromString(getStringFromJSON(j, "hash"));

            height = getUint64FromJSON(j, "height");
        }

        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.StartObject();
            {
                writer.Key("hash");
                hash.toJSON(writer);

                writer.Key("height");
                writer.Uint64(height);
            }
            writer.EndObject();
        }
    };

    class FeeType
    {
        public:
            /* Fee will be specified as fee per byte, for example, 1 atomic TRTL per byte. */
            bool isFeePerByte = false;

            /* Fee for each byte, in atomic units. Allowed to be a double, since
             * we will truncate it to an int upon performing the chunking. */
            double feePerByte = 0;

            /* Fee will be specified as a fixed fee */
            bool isFixedFee = false;

            /* Total fee to use */
            uint64_t fixedFee = 0;

            /* Fee will not be specified, use the minimum possible */
            bool isMinimumFee = false;

            static FeeType MinimumFee()
            {
                FeeType fee;
                fee.isMinimumFee = true;
                return fee;
            }

            static FeeType FeePerByte(const double feePerByte)
            {
                FeeType fee;
                fee.isFeePerByte = true;
                fee.feePerByte = feePerByte;
                return fee;
            }

            static FeeType FixedFee(const uint64_t fixedFee)
            {
                FeeType fee;
                fee.isFixedFee = true;
                fee.fixedFee = fixedFee;
                return fee;
            }

        private:
            FeeType() = default;
    };

    struct TransactionResult
    {
        /* The error, if any */
        Error error;

        /* The raw transaction */
        CryptoNote::Transaction transaction;

        /* The transaction outputs, before converted into boost uglyness, used
           for determining key inputs from the tx that belong to us */
        std::vector<WalletTypes::KeyOutput> outputs;

        /* The random key pair we generated */
        CryptoNote::KeyPair txKeyPair;
    };

    struct PreparedTransactionInfo
    {
        uint64_t fee;

        std::string paymentID;

        std::vector<WalletTypes::TxInputAndOwner> inputs;

        std::string changeAddress;

        uint64_t changeRequired;

        TransactionResult tx;

        Crypto::Hash transactionHash;
    };

    struct OutputEntry
    {
        uint32_t index;

        Crypto::PublicKey key;

        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.StartObject();
            {
                writer.Key("index");
                writer.Uint(index);

                writer.Key("key");
                key.toJSON(writer);
            }
            writer.EndObject();
        }

        void fromJSON(const JSONValue &j)
        {
            index = getUintFromJSON(j, "index");

            key.fromString(getStringFromJSON(j, "key"));
        }
    };

    struct RandomOuts
    {
        uint64_t amount;

        std::vector<OutputEntry> outputs;

        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.StartObject();
            {
                writer.Key("amount");
                writer.Uint64(amount);

                writer.Key("outputs");
                writer.StartArray();
                {
                    for (const auto &output : outputs)
                    {
                        output.toJSON(writer);
                    }
                }
                writer.EndArray();
            }
            writer.EndObject();
        }

        void fromJSON(const JSONValue &j)
        {
            amount = getUint64FromJSON(j, "amount");

            outputs.clear();

            for (const auto &output : getArrayFromJSON(j, "outputs"))
            {
                OutputEntry entry;

                entry.fromJSON(output);

                outputs.push_back(entry);
            }
        }
    };
}
