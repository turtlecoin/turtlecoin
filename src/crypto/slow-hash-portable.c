// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2014-2018, The Aeon Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

/* This file contains the portable version of the slow-hash routines
   for the CryptoNight hashing algorithm */

#if !(!defined NO_AES && (defined(__arm__) || defined(__aarch64__))) && !(!defined NO_AES && (defined(__x86_64__) || (defined(_MSC_VER) && defined(_WIN64))))
  #pragma message ("info: Using slow-hash-portable.c")

  #include "slow-hash-common.h"

void slow_hash_allocate_state(void)
{
    // Do nothing, this is just to maintain compatibility with the upgraded slow-hash.c
    return;
}

void slow_hash_free_state(void)
{
    // As above
    return;
}

static void (*const extra_hashes[4])(const void *, size_t, char *) =
{
    hash_extra_blake, hash_extra_groestl, hash_extra_jh, hash_extra_skein
};

extern int aesb_single_round(const uint8_t *in, uint8_t*out, const uint8_t *expandedKey);
extern int aesb_pseudo_round(const uint8_t *in, uint8_t *out, const uint8_t *expandedKey);

static size_t e2i(const uint8_t* a, size_t count)
{
    return (*((uint64_t*)a) / AES_BLOCK_SIZE) & (count - 1);
}

static void mul(const uint8_t* a, const uint8_t* b, uint8_t* res)
{
    uint64_t a0, b0;
    uint64_t hi, lo;

    a0 = SWAP64LE(((uint64_t*)a)[0]);
    b0 = SWAP64LE(((uint64_t*)b)[0]);
    lo = mul128(a0, b0, &hi);
    ((uint64_t*)res)[0] = SWAP64LE(hi);
    ((uint64_t*)res)[1] = SWAP64LE(lo);
}

static void sum_half_blocks(uint8_t* a, const uint8_t* b)
{
    uint64_t a0, a1, b0, b1;

    a0 = SWAP64LE(((uint64_t*)a)[0]);
    a1 = SWAP64LE(((uint64_t*)a)[1]);
    b0 = SWAP64LE(((uint64_t*)b)[0]);
    b1 = SWAP64LE(((uint64_t*)b)[1]);
    a0 += b0;
    a1 += b1;
    ((uint64_t*)a)[0] = SWAP64LE(a0);
    ((uint64_t*)a)[1] = SWAP64LE(a1);
}

  #define U64(x) ((uint64_t *) (x))

static void copy_block(uint8_t* dst, const uint8_t* src)
{
    memcpy(dst, src, AES_BLOCK_SIZE);
}

static void swap_blocks(uint8_t *a, uint8_t *b)
{
    uint64_t t[2];
    U64(t)[0] = U64(a)[0];
    U64(t)[1] = U64(a)[1];
    U64(a)[0] = U64(b)[0];
    U64(a)[1] = U64(b)[1];
    U64(b)[0] = U64(t)[0];
    U64(b)[1] = U64(t)[1];
}

static void xor_blocks(uint8_t* a, const uint8_t* b)
{
    size_t i;
    for (i = 0; i < AES_BLOCK_SIZE; i++)
    {
        a[i] ^= b[i];
    }
}

static void xor64(uint8_t* left, const uint8_t* right)
{
    size_t i;
    for (i = 0; i < 8; ++i)
    {
        left[i] ^= right[i];
    }
}

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

void cn_slow_hash(const void *data, size_t length, char *hash, int light, int variant, int prehashed, uint32_t page_size, uint32_t scratchpad, uint32_t iterations)
{
    uint32_t init_rounds = (scratchpad / INIT_SIZE_BYTE);
    uint32_t aes_rounds = (iterations / 2);
    size_t aes_init = (page_size / AES_BLOCK_SIZE);

  #ifndef FORCE_USE_HEAP
    uint8_t long_state[page_size];
  #else
    #pragma message ("warning: ACTIVATING FORCE_USE_HEAP IN portable slow-hash-portable.c")
    uint8_t *long_state = (uint8_t *)malloc(page_size);
  #endif

    union cn_slow_hash_state state;
    uint8_t text[INIT_SIZE_BYTE];
    uint8_t a[AES_BLOCK_SIZE];
    uint8_t b[AES_BLOCK_SIZE * 2];
    uint8_t c1[AES_BLOCK_SIZE];
    uint8_t c2[AES_BLOCK_SIZE];
    uint8_t d[AES_BLOCK_SIZE];
    size_t i, j;
    uint8_t aes_key[AES_KEY_SIZE];
    oaes_ctx *aes_ctx;

    if (prehashed)
    {
      memcpy(&state.hs, data, length);
    }
    else
    {
      hash_process(&state.hs, data, length);
    }

    memcpy(text, state.init, INIT_SIZE_BYTE);
    memcpy(aes_key, state.hs.b, AES_KEY_SIZE);
    aes_ctx = (oaes_ctx *) oaes_alloc();

    VARIANT1_PORTABLE_INIT();
    VARIANT2_PORTABLE_INIT();

    oaes_key_import_data(aes_ctx, aes_key, AES_KEY_SIZE);

    for (i = 0; i < init_rounds; i++)
    {
      for (j = 0; j < INIT_SIZE_BLK; j++)
      {
          aesb_pseudo_round(&text[AES_BLOCK_SIZE * j], &text[AES_BLOCK_SIZE * j], aes_ctx->key->exp_data);
      }

      memcpy(&long_state[i * INIT_SIZE_BYTE], text, INIT_SIZE_BYTE);
    }

    for (i = 0; i < AES_BLOCK_SIZE; i++)
    {
        a[i] = state.k[     i] ^ state.k[AES_BLOCK_SIZE * 2 + i];
        b[i] = state.k[AES_BLOCK_SIZE + i] ^ state.k[AES_BLOCK_SIZE * 3 + i];
    }

    for (i = 0; i < aes_rounds; i++)
    {
        /* Dependency chain: address -> read value ------+
         * written value <-+ hard function (AES or MUL) <+
         * next address  <-+
         */
        /* Iteration 1 */
        j = e2i(a, aes_init);
        copy_block(c1, &long_state[j]);
        aesb_single_round(c1, c1, a);
        VARIANT2_PORTABLE_SHUFFLE_ADD(long_state, j);
        copy_block(&long_state[j], c1);
        xor_blocks(&long_state[j], b);
        assert(j == e2i(a, aes_init));
        VARIANT1_1(&long_state[j]);
        /* Iteration 2 */
        j = e2i(c1, aes_init);
        copy_block(c2, &long_state[j]);
        VARIANT2_PORTABLE_INTEGER_MATH(c2, c1);
        mul(c1, c2, d);
        VARIANT2_2_PORTABLE();
        VARIANT2_PORTABLE_SHUFFLE_ADD(long_state, j);
        swap_blocks(a, c1);
        sum_half_blocks(c1, d);
        swap_blocks(c1, c2);
        xor_blocks(c1, c2);
        VARIANT1_2(c2 + 8);
        copy_block(&long_state[j], c2);
        assert(j == e2i(a, aes_init));

        if (variant == 2)
        {
            copy_block(b + AES_BLOCK_SIZE, b);
        }

        copy_block(b, a);
        copy_block(a, c1);
    }

    memcpy(text, state.init, INIT_SIZE_BYTE);
    oaes_key_import_data(aes_ctx, &state.hs.b[32], AES_KEY_SIZE);

    for (i = 0; i < init_rounds; i++)
    {
        for (j = 0; j < INIT_SIZE_BLK; j++)
        {
            xor_blocks(&text[j * AES_BLOCK_SIZE], &long_state[i * INIT_SIZE_BYTE + j * AES_BLOCK_SIZE]);
            aesb_pseudo_round(&text[AES_BLOCK_SIZE * j], &text[AES_BLOCK_SIZE * j], aes_ctx->key->exp_data);
        }
    }

    memcpy(state.init, text, INIT_SIZE_BYTE);
    hash_permutation(&state.hs);
    /*memcpy(hash, &state, 32);*/
    extra_hashes[state.hs.b[0] & 3](&state, 200, hash);
    oaes_free((OAES_CTX **) &aes_ctx);

  #ifdef FORCE_USE_HEAP
    free(long_state);
  #endif
}

#endif
