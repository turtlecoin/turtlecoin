// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <limits>
#include <string>
#include <vector>
#include "CryptoNote.h"

namespace CryptoNote
{

    const size_t WALLET_INVALID_TRANSACTION_ID = std::numeric_limits<size_t>::max();

    const size_t WALLET_INVALID_TRANSFER_ID = std::numeric_limits<size_t>::max();

    const uint32_t WALLET_UNCONFIRMED_TRANSACTION_HEIGHT = std::numeric_limits<uint32_t>::max();

    enum class WalletTransactionState : uint8_t
    {
            SUCCEEDED = 0,
            FAILED,
            CANCELLED,
            CREATED,
            DELETED
    };

    enum WalletEventType
    {
        TRANSACTION_CREATED,
        TRANSACTION_UPDATED,
        BALANCE_UNLOCKED,
        SYNC_PROGRESS_UPDATED,
        SYNC_COMPLETED,
    };

    enum class WalletSaveLevel : uint8_t
    {
            SAVE_KEYS_ONLY,
            SAVE_KEYS_AND_TRANSACTIONS,
            SAVE_ALL
    };

    struct WalletTransactionCreatedData
    {
        size_t transactionIndex;
    };

    struct WalletTransactionUpdatedData
    {
        size_t transactionIndex;
    };

    struct WalletSynchronizationProgressUpdated
    {
        uint32_t processedBlockCount;
        uint32_t totalBlockCount;
    };

    struct WalletEvent
    {
        WalletEventType type;
        union
        {
            WalletTransactionCreatedData transactionCreated;
            WalletTransactionUpdatedData transactionUpdated;
            WalletSynchronizationProgressUpdated synchronizationProgressUpdated;
        };
    };

    struct WalletTransaction
    {
        WalletTransactionState state;
        uint64_t timestamp;
        uint32_t blockHeight;
        Crypto::Hash hash;
        int64_t totalAmount;
        uint64_t fee;
        uint64_t creationTime;
        uint64_t unlockTime;
        std::string extra;
        bool isBase;
    };

    enum class WalletTransferType : uint8_t
    {
            USUAL = 0,
            DONATION,
            CHANGE
    };

    struct WalletOrder
    {
        std::string address;
        uint64_t amount;
    };

    struct WalletTransfer
    {
        WalletTransferType type;
        std::string address;
        int64_t amount;
    };

    struct DonationSettings
    {
        std::string address;
        uint64_t threshold = 0;
    };

    struct TransactionParameters
    {
        std::vector<std::string> sourceAddresses;
        std::vector<WalletOrder> destinations;
        uint64_t fee = 0;
        uint16_t mixIn = 0;
        std::string extra;
        uint64_t unlockTimestamp = 0;
        DonationSettings donation;
        std::string changeDestination;
    };

    struct WalletTransactionWithTransfers
    {
        WalletTransaction transaction;
        std::vector<WalletTransfer> transfers;
    };

    struct TransactionsInBlockInfo
    {
        Crypto::Hash blockHash;
        std::vector<WalletTransactionWithTransfers> transactions;
    };

}
