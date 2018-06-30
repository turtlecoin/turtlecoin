// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <emmintrin.h>
#include <wmmintrin.h>

#if defined(_MSC_VER)
#include <intrin.h>
#else

#include <cpuid.h>

#endif

#include "aesb.h"
#include "initializer.h"
#include "Common/int-util.h"
#include "hash-ops.h"
#include "oaes_lib.h"
#include "cryptonight.h"
#include "cryptonight-variants.h"

void (*cn_slow_hash_fp)(void *, const void *, size_t, void *, int lite, int variant);

void cn_slow_hash_f(void *a, const void *b, size_t c, void *d, int lite, int variant)
{
    (*cn_slow_hash_fp)(a, b, c, d, lite, variant);
}

#if defined(__GNUC__)
#define likely(x) (__builtin_expect(!!(x), 1))
#define unlikely(x) (__builtin_expect(!!(x), 0))
#else
#define likely(x) (x)
#define unlikely(x) (x)
#define __attribute__(x)
#endif

#if defined(_MSC_VER)
#define restrict
#endif

#pragma pack(push, 1)
union cn_slow_hash_state
{
    union hash_state hs;
    struct
    {
        uint8_t k[64];
        uint8_t init[INIT_SIZE_BYTE];
    };
};
#pragma pack(pop)

#if defined(_MSC_VER)
#define ALIGNED_DATA(x) __declspec(align(x))
#define ALIGNED_DECL(t, x) ALIGNED_DATA(x) t
#elif defined(__GNUC__)
#define ALIGNED_DATA(x) __attribute__((aligned(x)))
#define ALIGNED_DECL(t, x) t ALIGNED_DATA(x)
#endif

// Suml: This is not an ideal situation. What's happening right now is that a
// flat blob of data is allocated as a flat blob of data in slow-hash.cpp and
// then re-casted to this structure in slow-hash.inl.

struct cn_ctx
{
    ALIGNED_DECL(uint8_t long_state[MEMORY], 16);
    ALIGNED_DECL(union cn_slow_hash_state state, 16);
    ALIGNED_DECL(uint8_t text[INIT_SIZE_BYTE], 16);
    ALIGNED_DECL(uint64_t a[AES_BLOCK_SIZE >> 3], 16);
    ALIGNED_DECL(uint64_t b[AES_BLOCK_SIZE >> 3], 16);
    ALIGNED_DECL(uint8_t c[AES_BLOCK_SIZE], 16);
    oaes_ctx *aes_ctx;
};

// Suml: Unused now because of how oddly the cn_context is allocated.

struct cn_ctx_lite
{
    ALIGNED_DECL(uint8_t long_state[LITE_MEMORY], 16);
    ALIGNED_DECL(union cn_slow_hash_state state, 16);
    ALIGNED_DECL(uint8_t text[INIT_SIZE_BYTE], 16);
    ALIGNED_DECL(uint64_t a[AES_BLOCK_SIZE >> 3], 16);
    ALIGNED_DECL(uint64_t b[AES_BLOCK_SIZE >> 3], 16);
    ALIGNED_DECL(uint8_t c[AES_BLOCK_SIZE], 16);
    oaes_ctx *aes_ctx;
};

static_assert(sizeof(struct cn_ctx) == SLOW_HASH_CONTEXT_SIZE, "Invalid structure size");
static_assert(sizeof(struct cn_ctx_lite) == SLOW_HASH_CONTEXT_LITE_SIZE, "Invalid structure size");

static inline void ExpandAESKey256_sub1(__m128i *tmp1, __m128i *tmp2)
{
    __m128i tmp4;
    *tmp2 = _mm_shuffle_epi32(*tmp2, 0xFF);
    tmp4 = _mm_slli_si128(*tmp1, 0x04);
    *tmp1 = _mm_xor_si128(*tmp1, tmp4);
    tmp4 = _mm_slli_si128(tmp4, 0x04);
    *tmp1 = _mm_xor_si128(*tmp1, tmp4);
    tmp4 = _mm_slli_si128(tmp4, 0x04);
    *tmp1 = _mm_xor_si128(*tmp1, tmp4);
    *tmp1 = _mm_xor_si128(*tmp1, *tmp2);
}

static inline void ExpandAESKey256_sub2(__m128i *tmp1, __m128i *tmp3)
{
    __m128i tmp2, tmp4;

    tmp4 = _mm_aeskeygenassist_si128(*tmp1, 0x00);
    tmp2 = _mm_shuffle_epi32(tmp4, 0xAA);
    tmp4 = _mm_slli_si128(*tmp3, 0x04);
    *tmp3 = _mm_xor_si128(*tmp3, tmp4);
    tmp4 = _mm_slli_si128(tmp4, 0x04);
    *tmp3 = _mm_xor_si128(*tmp3, tmp4);
    tmp4 = _mm_slli_si128(tmp4, 0x04);
    *tmp3 = _mm_xor_si128(*tmp3, tmp4);
    *tmp3 = _mm_xor_si128(*tmp3, tmp2);
}

// Special thanks to Intel for helping me
// with ExpandAESKey256() and its subroutines
static inline void ExpandAESKey256(uint8_t *keybuf)
{
    __m128i tmp1, tmp2, tmp3, *keys;

    keys = (__m128i *) keybuf;

    tmp1 = _mm_load_si128((__m128i *) keybuf);
    tmp3 = _mm_load_si128((__m128i *) (keybuf + 0x10));

    tmp2 = _mm_aeskeygenassist_si128(tmp3, 0x01);
    ExpandAESKey256_sub1(&tmp1, &tmp2);
    keys[2] = tmp1;
    ExpandAESKey256_sub2(&tmp1, &tmp3);
    keys[3] = tmp3;

    tmp2 = _mm_aeskeygenassist_si128(tmp3, 0x02);
    ExpandAESKey256_sub1(&tmp1, &tmp2);
    keys[4] = tmp1;
    ExpandAESKey256_sub2(&tmp1, &tmp3);
    keys[5] = tmp3;

    tmp2 = _mm_aeskeygenassist_si128(tmp3, 0x04);
    ExpandAESKey256_sub1(&tmp1, &tmp2);
    keys[6] = tmp1;
    ExpandAESKey256_sub2(&tmp1, &tmp3);
    keys[7] = tmp3;

    tmp2 = _mm_aeskeygenassist_si128(tmp3, 0x08);
    ExpandAESKey256_sub1(&tmp1, &tmp2);
    keys[8] = tmp1;
    ExpandAESKey256_sub2(&tmp1, &tmp3);
    keys[9] = tmp3;

    tmp2 = _mm_aeskeygenassist_si128(tmp3, 0x10);
    ExpandAESKey256_sub1(&tmp1, &tmp2);
    keys[10] = tmp1;
    ExpandAESKey256_sub2(&tmp1, &tmp3);
    keys[11] = tmp3;

    tmp2 = _mm_aeskeygenassist_si128(tmp3, 0x20);
    ExpandAESKey256_sub1(&tmp1, &tmp2);
    keys[12] = tmp1;
    ExpandAESKey256_sub2(&tmp1, &tmp3);
    keys[13] = tmp3;

    tmp2 = _mm_aeskeygenassist_si128(tmp3, 0x40);
    ExpandAESKey256_sub1(&tmp1, &tmp2);
    keys[14] = tmp1;
}

static void (*const extra_hashes[4])(const void *, size_t, char *) =
        {
                hash_extra_blake, hash_extra_groestl, hash_extra_jh, hash_extra_skein
        };

#include "slow-hash.inl"

#define AESNI

#include "slow-hash.inl"

INITIALIZER(detect_aes)
{
    int ecx;
#if defined(_MSC_VER)
    int cpuinfo[4];
    __cpuid(cpuinfo, 1);
    ecx = cpuinfo[2];
#else
    int a, b, d;
    __cpuid(1, a, b, ecx, d);
#endif
    cn_slow_hash_fp = (ecx & (1 << 25)) ? &cn_slow_hash_aesni : &cn_slow_hash_noaesni;
}
