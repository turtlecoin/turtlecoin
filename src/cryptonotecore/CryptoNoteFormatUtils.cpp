// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "CryptoNoteFormatUtils.h"

#include "CryptoNoteBasicImpl.h"
#include "common/CryptoNoteTools.h"
#include "serialization/BinaryInputStreamSerializer.h"
#include "serialization/BinaryOutputStreamSerializer.h"
#include "serialization/CryptoNoteSerialization.h"

#include <common/Varint.h>
#include <config/CryptoNoteConfig.h>
#include <logging/LoggerRef.h>
#include <set>

using namespace Logging;
using namespace Crypto;
using namespace Common;

namespace CryptoNote
{
    bool generate_key_image_helper(
        const AccountKeys &ack,
        const PublicKey &tx_public_key,
        size_t real_output_index,
        KeyPair &in_ephemeral,
        KeyImage &ki)
    {
        KeyDerivation recv_derivation;
        bool r = generate_key_derivation(tx_public_key, ack.viewSecretKey, recv_derivation);

        assert(r && "key image helper: failed to generate_key_derivation");

        if (!r)
        {
            return false;
        }

        r = derive_public_key(recv_derivation, real_output_index, ack.address.spendPublicKey, in_ephemeral.publicKey);

        assert(r && "key image helper: failed to derive_public_key");

        if (!r)
        {
            return false;
        }

        derive_secret_key(recv_derivation, real_output_index, ack.spendSecretKey, in_ephemeral.secretKey);
        generate_key_image(in_ephemeral.publicKey, in_ephemeral.secretKey, ki);
        return true;
    }

    bool get_tx_fee(const Transaction &tx, uint64_t &fee)
    {
        uint64_t amount_in = 0;
        uint64_t amount_out = 0;

        for (const auto &in : tx.inputs)
        {
            if (in.type() == typeid(KeyInput))
            {
                amount_in += boost::get<KeyInput>(in).amount;
            }
        }

        for (const auto &o : tx.outputs)
        {
            amount_out += o.amount;
        }

        if (!(amount_in >= amount_out))
        {
            return false;
        }

        fee = amount_in - amount_out;
        return true;
    }

    uint64_t get_tx_fee(const Transaction &tx)
    {
        uint64_t r = 0;
        if (!get_tx_fee(tx, r))
        {
            return 0;
        }
        return r;
    }

    std::vector<uint32_t> relativeOutputOffsetsToAbsolute(const std::vector<uint32_t> &off)
    {
        std::vector<uint32_t> res = off;
        for (size_t i = 1; i < res.size(); i++)
        {
            res[i] += res[i - 1];
        }
        return res;
    }

    std::vector<uint32_t> absolute_output_offsets_to_relative(const std::vector<uint32_t> &off)
    {
        if (off.empty())
        {
            return {};
        }
        auto copy = off;
        for (size_t i = 1; i < copy.size(); ++i)
        {
            copy[i] = off[i] - off[i - 1];
        }
        return copy;
    }

    bool checkInputTypesSupported(const TransactionPrefix &tx)
    {
        for (const auto &in : tx.inputs)
        {
            if (in.type() != typeid(KeyInput))
            {
                return false;
            }
        }

        return true;
    }

    bool checkOutsValid(const TransactionPrefix &tx, std::string *error)
    {
        for (const TransactionOutput &out : tx.outputs)
        {
            if (out.target.type() == typeid(KeyOutput))
            {
                if (out.amount == 0)
                {
                    if (error)
                    {
                        *error = "Zero amount ouput";
                    }
                    return false;
                }

                if (!check_key(boost::get<KeyOutput>(out.target).key))
                {
                    if (error)
                    {
                        *error = "Output with invalid key";
                    }
                    return false;
                }
            }
            else
            {
                if (error)
                {
                    *error = "Output with invalid type";
                }
                return false;
            }
        }

        return true;
    }

    bool checkInputsOverflow(const TransactionPrefix &tx)
    {
        uint64_t money = 0;

        for (const auto &in : tx.inputs)
        {
            uint64_t amount = 0;

            if (in.type() == typeid(KeyInput))
            {
                amount = boost::get<KeyInput>(in).amount;
            }

            if (money > amount + money)
            {
                return false;
            }

            money += amount;
        }
        return true;
    }

    bool checkOutsOverflow(const TransactionPrefix &tx)
    {
        uint64_t money = 0;
        for (const auto &o : tx.outputs)
        {
            if (money > o.amount + money)
            {
                return false;
            }
            money += o.amount;
        }
        return true;
    }

    bool is_out_to_acc(
        const AccountKeys &acc,
        const KeyOutput &out_key,
        const KeyDerivation &derivation,
        size_t keyIndex)
    {
        PublicKey pk;
        derive_public_key(derivation, keyIndex, acc.address.spendPublicKey, pk);
        return pk == out_key.key;
    }

    bool is_out_to_acc(const AccountKeys &acc, const KeyOutput &out_key, const PublicKey &tx_pub_key, size_t keyIndex)
    {
        KeyDerivation derivation;
        generate_key_derivation(tx_pub_key, acc.viewSecretKey, derivation);
        return is_out_to_acc(acc, out_key, derivation, keyIndex);
    }

    uint64_t getInputAmount(const Transaction &transaction)
    {
        uint64_t amount = 0;
        for (auto &input : transaction.inputs)
        {
            if (input.type() == typeid(KeyInput))
            {
                amount += boost::get<KeyInput>(input).amount;
            }
        }

        return amount;
    }

    std::vector<uint64_t> getInputsAmounts(const Transaction &transaction)
    {
        std::vector<uint64_t> inputsAmounts;
        inputsAmounts.reserve(transaction.inputs.size());

        for (auto &input : transaction.inputs)
        {
            if (input.type() == typeid(KeyInput))
            {
                inputsAmounts.push_back(boost::get<KeyInput>(input).amount);
            }
        }

        return inputsAmounts;
    }

    uint64_t getOutputAmount(const Transaction &transaction)
    {
        uint64_t amount = 0;
        for (auto &output : transaction.outputs)
        {
            amount += output.amount;
        }

        return amount;
    }

    void decomposeAmount(uint64_t amount, uint64_t dustThreshold, std::vector<uint64_t> &decomposedAmounts)
    {
        decompose_amount_into_digits(
            amount,
            dustThreshold,
            [&](uint64_t amount) { decomposedAmounts.push_back(amount); },
            [&](uint64_t dust) { decomposedAmounts.push_back(dust); });
    }

} // namespace CryptoNote
