// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "KeysStorage.h"

#include "WalletLegacy/WalletLegacySerialization.h"
#include "Serialization/ISerializer.h"
#include "Serialization/SerializationOverloads.h"
#include "CryptoNoteCore/CryptoNoteSerialization.h"

namespace CryptoNote
{

    void KeysStorage::serialize(ISerializer &serializer, const std::string &name)
    {
        serializer.beginObject(name);

        serializer(creationTimestamp, "creation_timestamp");

        serializer(spendPublicKey, "spend_public_key");
        serializer(spendSecretKey, "spend_secret_key");

        serializer(viewPublicKey, "view_public_key");
        serializer(viewSecretKey, "view_secret_key");

        serializer.endObject();
    }

}
