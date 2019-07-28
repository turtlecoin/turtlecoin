// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoNote.h>
#include <functional>

namespace CryptoNote
{
    enum class TransactionMessageType
    {
        AddTransactionType,
        DeleteTransactionType
    };

    // immutable messages
    struct AddTransaction
    {
        Crypto::Hash hash;
    };

    struct DeleteTransaction
    {
        Crypto::Hash hash;
    };

    class TransactionPoolMessage
    {
      public:
        TransactionPoolMessage(const AddTransaction &at);

        TransactionPoolMessage(const DeleteTransaction &at);

        // pattern matchin API
        void match(std::function<void(const AddTransaction &)> &&, std::function<void(const DeleteTransaction &)> &&);

        // API with explicit type handling
        TransactionMessageType getType() const;

        AddTransaction getAddTransaction() const;

        DeleteTransaction getDeleteTransaction() const;

      private:
        const TransactionMessageType type;

        union {
            const AddTransaction addTransaction;
            const DeleteTransaction deleteTransaction;
        };
    };

} // namespace CryptoNote
