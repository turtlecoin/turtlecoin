// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "CryptoNoteBasic.h"

#include "crypto/crypto.h"

namespace CryptoNote
{
    KeyPair generateKeyPair()
    {
        KeyPair k;
        Crypto::generate_keys(k.publicKey, k.secretKey);
        return k;
    }

} // namespace CryptoNote
