// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "groestl.h"

#include <stddef.h>
#include <stdint.h>

void hash_extra_groestl(const void *data, size_t length, char *hash)
{
    groestl(data, length * 8, (uint8_t *)hash);
}
