// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "ITransaction.h"
#include "cryptonotecore/CryptoNoteBasic.h"

namespace CryptoNote
{
    bool checkInputsKeyimagesDiff(const CryptoNote::TransactionPrefix &tx);

    // TransactionInput helper functions
    size_t getRequiredSignaturesCount(const TransactionInput &in);

    uint64_t getTransactionInputAmount(const TransactionInput &in);

    TransactionTypes::InputType getTransactionInputType(const TransactionInput &in);

    const TransactionInput &getInputChecked(const CryptoNote::TransactionPrefix &transaction, size_t index);

    const TransactionInput &getInputChecked(
        const CryptoNote::TransactionPrefix &transaction,
        size_t index,
        TransactionTypes::InputType type);

    // TransactionOutput helper functions
    TransactionTypes::OutputType getTransactionOutputType(const TransactionOutputTarget &out);

    const TransactionOutput &getOutputChecked(const CryptoNote::TransactionPrefix &transaction, size_t index);

    const TransactionOutput &getOutputChecked(
        const CryptoNote::TransactionPrefix &transaction,
        size_t index,
        TransactionTypes::OutputType type);

    bool findOutputsToAccount(
        const CryptoNote::TransactionPrefix &transaction,
        const AccountPublicAddress &addr,
        const Crypto::SecretKey &viewSecretKey,
        std::vector<uint32_t> &out,
        uint64_t &amount);

} // namespace CryptoNote
