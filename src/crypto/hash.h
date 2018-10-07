// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2014-2018, The Aeon Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <stddef.h>

#include <CryptoTypes.h>
#include "generic-ops.h"

// CryptoNight Defintions Below

// Standard Cryptonight Definitions
#define CN_MEMORY                       2097152 // 2MB scratchpad 2^21
#define CN_ITER                         1048576 // 2^20

// Cryptonight Lite Definitions
#define CN_LITE_MEMORY                  1048576 // 1MB scratchpad 2^20
#define CN_LITE_ITER                    524288 // 2^19

// CryptoNight Soft Shell Definitions
#define CN_SOFT_SHELL_MEMORY            262144 // 256K scratchpad 2^18
#define CN_SOFT_SHELL_ITER              131072 // 2^17
#define CN_SOFT_SHELL_WINDOW            2048 // This defines how many blocks we cycle through as part of our algo sine wave
#define CN_SOFT_SHELL_MULTIPLIER        3 // This defines how big our steps are for each block and
                                          // ultimately determines how big our sine wave is. A smaller value means a bigger wave
#define CN_SOFT_SHELL_PAD_MULTIPLIER    (CN_SOFT_SHELL_WINDOW / CN_SOFT_SHELL_MULTIPLIER)
#define CN_SOFT_SHELL_ITER_MULTIPLIER   (CN_SOFT_SHELL_PAD_MULTIPLIER / 2) // This value should always be half of our pad multiplier

#if (((CN_SOFT_SHELL_WINDOW * CN_SOFT_SHELL_PAD_MULTIPLIER) + CN_SOFT_SHELL_MEMORY) > CN_MEMORY)
#error The CryptoNight Soft Shell Parameters you supplied will exceed normal paging operations.
#endif

namespace Crypto {

  extern "C" {
#include "hash-ops.h"
  }

  /*
    Cryptonight hash functions
  */

  inline void cn_fast_hash(const void *data, size_t length, Hash &hash) {
    cn_fast_hash(data, length, reinterpret_cast<char *>(&hash));
  }

  inline Hash cn_fast_hash(const void *data, size_t length) {
    Hash h;
    cn_fast_hash(data, length, reinterpret_cast<char *>(&h));
    return h;
  }

  inline void cn_slow_hash_v0(const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 0, 0, 0, CN_MEMORY, CN_ITER);
  }

  inline void cn_slow_hash_v1(const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 0, 1, 0, CN_MEMORY, CN_ITER);
  }

  inline void cn_slow_hash_v2(const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 0, 2, 0, CN_MEMORY, CN_ITER);
  }

  inline void cn_lite_slow_hash_v0(const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 1, 0, 0, CN_LITE_MEMORY, CN_LITE_ITER);
  }

  inline void cn_lite_slow_hash_v1(const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 1, 1, 0, CN_LITE_MEMORY, CN_LITE_ITER);
  }

  inline void cn_lite_slow_hash_v2(const void *data, size_t length, Hash &hash) {
    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 1, 2, 0, CN_LITE_MEMORY, CN_LITE_ITER);
  }

  inline void cn_soft_shell_slow_hash_v0(const void *data, size_t length, Hash &hash, uint32_t height) {
    uint32_t base_offset = (height % CN_SOFT_SHELL_WINDOW);
    int32_t offset = (height % (CN_SOFT_SHELL_WINDOW * 2)) - (base_offset * 2);
    if (offset < 0) {
      offset = base_offset;
    }

    uint32_t scratchpad = CN_SOFT_SHELL_MEMORY + (static_cast<uint32_t>(offset) * CN_SOFT_SHELL_PAD_MULTIPLIER);
    uint32_t iterations = CN_SOFT_SHELL_ITER + (static_cast<uint32_t>(offset) * CN_SOFT_SHELL_ITER_MULTIPLIER);

    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 1, 0, 0, scratchpad, iterations);
  }

  inline void cn_soft_shell_slow_hash_v1(const void *data, size_t length, Hash &hash, uint32_t height) {
    uint32_t base_offset = (height % CN_SOFT_SHELL_WINDOW);
    int32_t offset = (height % (CN_SOFT_SHELL_WINDOW * 2)) - (base_offset * 2);
    if (offset < 0) {
      offset = base_offset;
    }

    uint32_t scratchpad = CN_SOFT_SHELL_MEMORY + (static_cast<uint32_t>(offset) * CN_SOFT_SHELL_PAD_MULTIPLIER);
    uint32_t iterations = CN_SOFT_SHELL_ITER + (static_cast<uint32_t>(offset) * CN_SOFT_SHELL_ITER_MULTIPLIER);

    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 1, 1, 0, scratchpad, iterations);
  }

  inline void cn_soft_shell_slow_hash_v2(const void *data, size_t length, Hash &hash, uint32_t height) {
    uint32_t base_offset = (height % CN_SOFT_SHELL_WINDOW);
    int32_t offset = (height % (CN_SOFT_SHELL_WINDOW * 2)) - (base_offset * 2);
    if (offset < 0) {
      offset = base_offset;
    }

    uint32_t scratchpad = CN_SOFT_SHELL_MEMORY + (static_cast<uint32_t>(offset) * CN_SOFT_SHELL_PAD_MULTIPLIER);
    uint32_t iterations = CN_SOFT_SHELL_ITER + (static_cast<uint32_t>(offset) * CN_SOFT_SHELL_ITER_MULTIPLIER);

    cn_slow_hash(data, length, reinterpret_cast<char *>(&hash), 1, 2, 0, scratchpad, iterations);
  }

  inline void tree_hash(const Hash *hashes, size_t count, Hash &root_hash) {
    tree_hash(reinterpret_cast<const char (*)[HASH_SIZE]>(hashes), count, reinterpret_cast<char *>(&root_hash));
  }

  inline void tree_branch(const Hash *hashes, size_t count, Hash *branch) {
    tree_branch(reinterpret_cast<const char (*)[HASH_SIZE]>(hashes), count, reinterpret_cast<char (*)[HASH_SIZE]>(branch));
  }

  inline void tree_hash_from_branch(const Hash *branch, size_t depth, const Hash &leaf, const void *path, Hash &root_hash) {
    tree_hash_from_branch(reinterpret_cast<const char (*)[HASH_SIZE]>(branch), depth, reinterpret_cast<const char *>(&leaf), path, reinterpret_cast<char *>(&root_hash));
  }
}

CRYPTO_MAKE_HASHABLE(Hash)
