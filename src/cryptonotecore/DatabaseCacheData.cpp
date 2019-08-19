// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <common/CryptoNoteTools.h>
#include <cryptonotecore/DatabaseCacheData.h>
#include <serialization/CryptoNoteSerialization.h>
#include <serialization/SerializationOverloads.h>

namespace CryptoNote
{
    void ExtendedTransactionInfo::serialize(CryptoNote::ISerializer &s)
    {
        s(static_cast<CachedTransactionInfo &>(*this), "cached_transaction");
        s(amountToKeyIndexes, "key_indexes");
    }

    void KeyOutputInfo::serialize(ISerializer &s)
    {
        s(publicKey, "public_key");
        s(transactionHash, "transaction_hash");
        s(unlockTime, "unlock_time");
        s(outputIndex, "output_index");
    }

} // namespace CryptoNote
