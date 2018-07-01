// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "crypto/crypto.h"

#include <stdint.h>

namespace CryptoNote
{

    class ISerializer;

//This is DTO structure. Do not change it.
    struct KeysStorage
    {
        uint64_t creationTimestamp;

        Crypto::PublicKey spendPublicKey;
        Crypto::SecretKey spendSecretKey;

        Crypto::PublicKey viewPublicKey;
        Crypto::SecretKey viewSecretKey;

        void serialize(ISerializer &serializer, const std::string &name);
    };

} //namespace CryptoNote
