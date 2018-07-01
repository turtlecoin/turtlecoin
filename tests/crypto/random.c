// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "crypto/random.c"

#include "crypto-tests.h"

void setup_random(void) {
    memset(&state, 42, sizeof(union hash_state));
}
