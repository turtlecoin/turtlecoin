// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IObservable.h"
#include "IStreamSerializable.h"
#include "ITransaction.h"
#include "crypto/hash.h"

#include <cstdint>
#include <limits>
#include <vector>

namespace CryptoNote
{
    const uint32_t UNCONFIRMED_TRANSACTION_GLOBAL_OUTPUT_INDEX = std::numeric_limits<uint32_t>::max();

    struct TransactionInformation
    {
        // transaction info
        Crypto::Hash transactionHash;
        Crypto::PublicKey publicKey;
        uint32_t blockHeight;
        uint64_t timestamp;
        uint64_t unlockTime;
        uint64_t totalAmountIn;
        uint64_t totalAmountOut;
        std::vector<uint8_t> extra;
        Crypto::Hash paymentId;
    };

    struct TransactionOutputInformation
    {
        // output info
        TransactionTypes::OutputType type;
        uint64_t amount;
        uint32_t globalOutputIndex;
        uint32_t outputInTransaction;

        // transaction info
        Crypto::Hash transactionHash;
        Crypto::PublicKey transactionPublicKey;

        Crypto::PublicKey outputKey;
    };

    struct TransactionSpentOutputInformation : public TransactionOutputInformation
    {
        uint32_t spendingBlockHeight;
        uint64_t timestamp;
        Crypto::Hash spendingTransactionHash;
        Crypto::KeyImage keyImage; //!< \attention Used only for TransactionTypes::OutputType::Key
        uint32_t inputInTransaction;
    };

    struct SpentTransactionOutput;

    class ITransfersContainer : public IStreamSerializable
    {
      public:
        enum Flags : uint32_t
        {
            // state
            IncludeStateUnlocked = 0x01,
            IncludeStateLocked = 0x02,
            IncludeStateSoftLocked = 0x04,
            IncludeStateSpent = 0x08,
            // output type
            IncludeTypeKey = 0x100,
            // combinations
            IncludeStateAll = 0xff,
            IncludeTypeAll = 0xff00,

            IncludeKeyUnlocked = IncludeTypeKey | IncludeStateUnlocked,
            IncludeKeyNotUnlocked = IncludeTypeKey | IncludeStateLocked | IncludeStateSoftLocked,

            IncludeAllLocked = IncludeTypeAll | IncludeStateLocked | IncludeStateSoftLocked,
            IncludeAllUnlocked = IncludeTypeAll | IncludeStateUnlocked,
            IncludeAll = IncludeTypeAll | IncludeStateAll,

            IncludeDefault = IncludeKeyUnlocked
        };

        virtual size_t transactionsCount() const = 0;

        virtual uint64_t balance(uint32_t flags = IncludeDefault) const = 0;

        virtual void
            getOutputs(std::vector<TransactionOutputInformation> &transfers, uint32_t flags = IncludeDefault) const = 0;

        virtual bool getTransactionInformation(
            const Crypto::Hash &transactionHash,
            TransactionInformation &info,
            uint64_t *amountIn = nullptr,
            uint64_t *amountOut = nullptr) const = 0;

        virtual std::vector<TransactionOutputInformation>
            getTransactionOutputs(const Crypto::Hash &transactionHash, uint32_t flags = IncludeDefault) const = 0;

        // only type flags are feasible for this function
        virtual std::vector<TransactionOutputInformation>
            getTransactionInputs(const Crypto::Hash &transactionHash, uint32_t flags) const = 0;

        virtual void getUnconfirmedTransactions(std::vector<Crypto::Hash> &transactions) const = 0;

        virtual std::vector<SpentTransactionOutput> getUnspentInputs() const = 0;

        virtual std::vector<SpentTransactionOutput> getSpentInputs() const = 0;
    };

} // namespace CryptoNote
