// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

// Cryptonight
#define MEMORY         (1 << 21) /* 2 MiB */
#define ITER           (1 << 19)
#define MASK           0x1FFFF0

// Cryptonight Lite
#define LITE_MEMORY    (1 << 20) /* 1 MiB */
#define LITE_ITER      (1 << 18)
#define LITE_MASK      0xFFFF0

#define AES_BLOCK_SIZE  16
#define AES_KEY_SIZE    32 /*16*/
#define INIT_SIZE_BLK   8
#define INIT_SIZE_BYTE (INIT_SIZE_BLK * AES_BLOCK_SIZE)    // 128

