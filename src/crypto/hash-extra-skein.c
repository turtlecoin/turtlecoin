// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "hash-ops.h"
#include "skein.h"

#include <stddef.h>
#include <stdint.h>

void hash_extra_skein(const void *data, size_t length, char *hash)
{
    int r = skein_hash(8 * HASH_SIZE, data, 8 * length, (uint8_t *)hash);
    if (r)
    {
    }
    assert(SKEIN_SUCCESS == r);
}
