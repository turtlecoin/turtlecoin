// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

/* The following was adapted from the Monero Cryptonight variant change of April 2018. */

#include <stdio.h>

#pragma once

static inline void xor64(uint64_t *a, const uint64_t b)
{
    *a ^= b;
}

#define VARIANT1_1(p) \
  do if (variant > 0) \
  { \
    const uint8_t tmp = ((const uint8_t*)(p))[11]; \
    static const uint32_t table = 0x75310; \
    const uint8_t index = (((tmp >> 3) & 6) | (tmp & 1)) << 1; \
    ((uint8_t*)(p))[11] = tmp ^ ((table >> index) & 0x30); \
  } while(0);

#define VARIANT1_2(p) \
  do if (variant > 0) \
  { \
    xor64(p, tweak1_2); \
  } while(0);

#define VARIANT1_CHECK() \
  do if (length < 43) \
  { \
    fprintf(stderr, "Cryptonight variants need at least 43 bytes of data. Aborting."); \
    abort(); \
  } while(0);

#define NONCE_POINTER (((const uint8_t*)data)+35)

#define VARIANT1_PORTABLE_INIT() \
  uint8_t tweak1_2[8]; \
  do if (variant > 0) \
  { \
    VARIANT1_CHECK(); \
    memcpy(&tweak1_2, &state.hs.b[192], sizeof(tweak1_2)); \
    xor64(tweak1_2, NONCE_POINTER); \
  } while(0)

#define VARIANT1_INIT64() \
  if (variant > 0) \
  { \
    VARIANT1_CHECK(); \
  } \
  const uint64_t tweak1_2 = variant > 0 ? (ctx->state.hs.w[24] ^ (*((const uint64_t*)NONCE_POINTER))) : 0
