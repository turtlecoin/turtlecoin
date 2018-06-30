// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "WalletLegacySerialization.h"
#include "WalletLegacy/WalletUnconfirmedTransactions.h"
#include "IWalletLegacy.h"

#include "CryptoNoteCore/CryptoNoteSerialization.h"
#include "Serialization/ISerializer.h"
#include "Serialization/SerializationOverloads.h"

namespace CryptoNote
{

    void serialize(UnconfirmedTransferDetails &utd, CryptoNote::ISerializer &serializer)
    {
        serializer(utd.tx, "transaction");
        serializer(utd.amount, "amount");
        serializer(utd.outsAmount, "outs_amount");
        uint64_t time = static_cast<uint64_t>(utd.sentTime);
        serializer(time, "sent_time");
        utd.sentTime = static_cast<time_t>(time);
        uint64_t txId = static_cast<uint64_t>(utd.transactionId);
        serializer(txId, "transaction_id");
        utd.transactionId = static_cast<size_t>(txId);
    }

    void serialize(WalletLegacyTransaction &txi, CryptoNote::ISerializer &serializer)
    {
        uint64_t trId = static_cast<uint64_t>(txi.firstTransferId);
        serializer(trId, "first_transfer_id");
        txi.firstTransferId = static_cast<size_t>(trId);

        uint64_t trCount = static_cast<uint64_t>(txi.transferCount);
        serializer(trCount, "transfer_count");
        txi.transferCount = static_cast<size_t>(trCount);

        serializer(txi.totalAmount, "total_amount");

        serializer(txi.fee, "fee");
        serializer(txi.hash, "hash");
        serializer(txi.isCoinbase, "is_coinbase");

        CryptoNote::serializeBlockHeight(serializer, txi.blockHeight, "block_height");

        serializer(txi.timestamp, "timestamp");
        serializer(txi.unlockTime, "unlock_time");
        serializer(txi.extra, "extra");

        //this field has been added later in the structure.
        //in order to not break backward binary compatibility
        // we just set it to zero
        txi.sentTime = 0;
    }

    void serialize(WalletLegacyTransfer &tr, CryptoNote::ISerializer &serializer)
    {
        serializer(tr.address, "address");
        serializer(tr.amount, "amount");
    }

} //namespace CryptoNote
