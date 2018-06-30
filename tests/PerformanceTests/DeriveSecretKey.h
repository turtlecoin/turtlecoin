// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "crypto/crypto.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"

#include "SingleTransactionTestBase.h"

class test_derive_secret_key : public single_tx_test_base
{
public:
    static const size_t loop_count = 1000000;

    bool init()
    {
        if (!single_tx_test_base::init())
            return false;

        Crypto::generate_key_derivation(m_tx_pub_key, m_bob.getAccountKeys().viewSecretKey, m_key_derivation);
        m_spend_secret_key = m_bob.getAccountKeys().spendSecretKey;

        return true;
    }

    bool test()
    {
        CryptoNote::KeyPair in_ephemeral;
        Crypto::derive_secret_key(m_key_derivation, 0, m_spend_secret_key, in_ephemeral.secretKey);
        return true;
    }

private:
    Crypto::KeyDerivation m_key_derivation;
    Crypto::SecretKey m_spend_secret_key;
};
