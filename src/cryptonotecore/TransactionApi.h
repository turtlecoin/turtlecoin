// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "ITransaction.h"

#include <memory>

namespace CryptoNote
{
    std::unique_ptr<ITransaction> createTransaction();

    std::unique_ptr<ITransaction> createTransaction(const BinaryArray &transactionBlob);

    std::unique_ptr<ITransaction> createTransaction(const Transaction &tx);

    std::unique_ptr<ITransactionReader>
        createTransactionPrefix(const TransactionPrefix &prefix, const Crypto::Hash &transactionHash);

    std::unique_ptr<ITransactionReader> createTransactionPrefix(const Transaction &fullTransaction);
} // namespace CryptoNote
