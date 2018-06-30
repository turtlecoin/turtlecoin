// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"

#include "SingleTransactionTestBase.h"

class test_generate_key_image_helper : public single_tx_test_base
{
public:
    static const size_t loop_count = 500;

    bool test()
    {
        CryptoNote::KeyPair in_ephemeral;
        Crypto::KeyImage ki;
        return CryptoNote::generate_key_image_helper(m_bob.getAccountKeys(), m_tx_pub_key, 0, in_ephemeral, ki);
    }
};
