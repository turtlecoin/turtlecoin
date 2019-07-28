// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoNote.h>
#include <boost/optional.hpp>

namespace CryptoNote
{
    class CachedTransaction
    {
      public:
        explicit CachedTransaction(Transaction &&transaction);

        explicit CachedTransaction(const Transaction &transaction);

        explicit CachedTransaction(const BinaryArray &transactionBinaryArray);

        const Transaction &getTransaction() const;

        const Crypto::Hash &getTransactionHash() const;

        const Crypto::Hash &getTransactionPrefixHash() const;

        const BinaryArray &getTransactionBinaryArray() const;

        uint64_t getTransactionFee() const;

      private:
        Transaction transaction;

        mutable boost::optional<BinaryArray> transactionBinaryArray;

        mutable boost::optional<Crypto::Hash> transactionHash;

        mutable boost::optional<Crypto::Hash> transactionPrefixHash;

        mutable boost::optional<uint64_t> transactionFee;
    };

} // namespace CryptoNote
