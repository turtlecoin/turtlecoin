// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2014-2018, The Aeon Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

/* This file contains the ARM versions of the CryptoNight slow-hash routines */

#if !defined NO_AES && (defined(__arm__) || defined(__aarch64__))
  #pragma message ("info: Using slow-hash-arm.c")

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

  #if defined(__GNUC__)
    #define RDATA_ALIGN16 __attribute__ ((aligned(16)))
    #define STATIC static
    #define INLINE inline
  #else /* defined(__GNUC__) */
    #define RDATA_ALIGN16
    #define STATIC static
    #define INLINE
  #endif /* defined(__GNUC__) */

  #define U64(x) ((uint64_t *) (x))

STATIC INLINE void xor64(uint64_t *a, const uint64_t b)
{
    *a ^= b;
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

  #if defined(__aarch64__) && defined(__ARM_FEATURE_CRYPTO)

/* ARMv8-A optimized with NEON and AES instructions.
 * Copied from the x86-64 AES-NI implementation. It has much the same
 * characteristics as x86-64: there's no 64x64=128 multiplier for vectors,
 * and moving between vector and regular registers stalls the pipeline.
 */
    #include <arm_neon.h>

    #define state_index(x,div) (((*((uint64_t *)x) >> 4) & (TOTALBLOCKS /(div) - 1)) << 4)
    #define __mul() __asm__("mul %0, %1, %2\n\t" : "=r"(lo) : "r"(c[0]), "r"(b[0]) ); \
  __asm__("umulh %0, %1, %2\n\t" : "=r"(hi) : "r"(c[0]), "r"(b[0]) );

    #define pre_aes() \
  j = state_index(a,lightFlag); \
  _c = vld1q_u8(&hp_state[j]); \
  _a = vld1q_u8((const uint8_t *)a); \

    #define post_aes() \
  VARIANT2_SHUFFLE_ADD_NEON(hp_state, j); \
  vst1q_u8((uint8_t *)c, _c); \
  vst1q_u8(&hp_state[j], veorq_u8(_b, _c)); \
  VARIANT1_1(&hp_state[j]); \
  j = state_index(c,lightFlag); \
  p = U64(&hp_state[j]); \
  b[0] = p[0]; b[1] = p[1]; \
  VARIANT2_PORTABLE_INTEGER_MATH(b, c); \
  __mul(); \
  VARIANT2_2(); \
  VARIANT2_SHUFFLE_ADD_NEON(hp_state, j); \
  a[0] += hi; a[1] += lo; \
  p = U64(&hp_state[j]); \
  p[0] = a[0];  p[1] = a[1]; \
  a[0] ^= b[0]; a[1] ^= b[1]; \
  VARIANT1_2(p + 1); \
  _b1 = _b; \
  _b = _c; \


/* Note: this was based on a standard 256bit key schedule but
 * it's been shortened since Cryptonight doesn't use the full
 * key schedule. Don't try to use this for vanilla AES.
*/
static void aes_expand_key(const uint8_t *key, uint8_t *expandedKey)
{
    static const int rcon[] =
    {
        0x01,0x01,0x01,0x01,
        0x0c0f0e0d,0x0c0f0e0d,0x0c0f0e0d,0x0c0f0e0d,	// rotate-n-splat
        0x1b,0x1b,0x1b,0x1b
    };

    __asm__(
        "	eor	v0.16b,v0.16b,v0.16b\n"
        "	ld1	{v3.16b},[%0],#16\n"
        "	ld1	{v1.4s,v2.4s},[%2],#32\n"
        "	ld1	{v4.16b},[%0]\n"
        "	mov	w2,#5\n"
        "	st1	{v3.4s},[%1],#16\n"
        "\n"
        "1:\n"
        "	tbl	v6.16b,{v4.16b},v2.16b\n"
        "	ext	v5.16b,v0.16b,v3.16b,#12\n"
        "	st1	{v4.4s},[%1],#16\n"
        "	aese	v6.16b,v0.16b\n"
        "	subs	w2,w2,#1\n"
        "\n"
        "	eor	v3.16b,v3.16b,v5.16b\n"
        "	ext	v5.16b,v0.16b,v5.16b,#12\n"
        "	eor	v3.16b,v3.16b,v5.16b\n"
        "	ext	v5.16b,v0.16b,v5.16b,#12\n"
        "	eor	v6.16b,v6.16b,v1.16b\n"
        "	eor	v3.16b,v3.16b,v5.16b\n"
        "	shl	v1.16b,v1.16b,#1\n"
        "	eor	v3.16b,v3.16b,v6.16b\n"
        "	st1	{v3.4s},[%1],#16\n"
        "	b.eq	2f\n"
        "\n"
        "	dup	v6.4s,v3.s[3]		// just splat\n"
        "	ext	v5.16b,v0.16b,v4.16b,#12\n"
        "	aese	v6.16b,v0.16b\n"
        "\n"
        "	eor	v4.16b,v4.16b,v5.16b\n"
        "	ext	v5.16b,v0.16b,v5.16b,#12\n"
        "	eor	v4.16b,v4.16b,v5.16b\n"
        "	ext	v5.16b,v0.16b,v5.16b,#12\n"
        "	eor	v4.16b,v4.16b,v5.16b\n"
        "\n"
        "	eor	v4.16b,v4.16b,v6.16b\n"
        "	b	1b\n"
        "\n"
        "2:\n" : : "r"(key),
        "r"(expandedKey),
        "r"(rcon)
    );
}

/* An ordinary AES round is a sequence of SubBytes, ShiftRows, MixColumns, AddRoundKey. There
 * is also an InitialRound which consists solely of AddRoundKey. The ARM instructions slice
 * this sequence differently; the aese instruction performs AddRoundKey, SubBytes, ShiftRows.
 * The aesmc instruction does the MixColumns. Since the aese instruction moves the AddRoundKey
 * up front, and Cryptonight's hash skips the InitialRound step, we have to kludge it here by
 * feeding in a vector of zeros for our first step. Also we have to do our own Xor explicitly
 * at the last step, to provide the AddRoundKey that the ARM instructions omit.
 */
STATIC INLINE void aes_pseudo_round(const uint8_t *in, uint8_t *out, const uint8_t *expandedKey, int nblocks)
{
    const uint8x16_t *k = (const uint8x16_t *)expandedKey, zero = {0};
    uint8x16_t tmp;
    int i;

    for (i=0; i<nblocks; i++)
    {
        uint8x16_t tmp = vld1q_u8(in + i * AES_BLOCK_SIZE);
        tmp = vaeseq_u8(tmp, zero);
        tmp = vaesmcq_u8(tmp);
        tmp = vaeseq_u8(tmp, k[0]);
        tmp = vaesmcq_u8(tmp);
        tmp = vaeseq_u8(tmp, k[1]);
        tmp = vaesmcq_u8(tmp);
        tmp = vaeseq_u8(tmp, k[2]);
        tmp = vaesmcq_u8(tmp);
        tmp = vaeseq_u8(tmp, k[3]);
        tmp = vaesmcq_u8(tmp);
        tmp = vaeseq_u8(tmp, k[4]);
        tmp = vaesmcq_u8(tmp);
        tmp = vaeseq_u8(tmp, k[5]);
        tmp = vaesmcq_u8(tmp);
        tmp = vaeseq_u8(tmp, k[6]);
        tmp = vaesmcq_u8(tmp);
        tmp = vaeseq_u8(tmp, k[7]);
        tmp = vaesmcq_u8(tmp);
        tmp = vaeseq_u8(tmp, k[8]);
        tmp = vaesmcq_u8(tmp);
        tmp = veorq_u8(tmp,  k[9]);
        vst1q_u8(out + i * AES_BLOCK_SIZE, tmp);
    }
}

STATIC INLINE void aes_pseudo_round_xor(const uint8_t *in, uint8_t *out, const uint8_t *expandedKey, const uint8_t *xor, int nblocks)
{
    const uint8x16_t *k = (const uint8x16_t *)expandedKey;
    const uint8x16_t *x = (const uint8x16_t *)xor;
    uint8x16_t tmp;
    int i;

    for (i=0; i<nblocks; i++)
    {
        uint8x16_t tmp = vld1q_u8(in + i * AES_BLOCK_SIZE);
        tmp = vaeseq_u8(tmp, x[i]);
        tmp = vaesmcq_u8(tmp);
        tmp = vaeseq_u8(tmp, k[0]);
        tmp = vaesmcq_u8(tmp);
        tmp = vaeseq_u8(tmp, k[1]);
        tmp = vaesmcq_u8(tmp);
        tmp = vaeseq_u8(tmp, k[2]);
        tmp = vaesmcq_u8(tmp);
        tmp = vaeseq_u8(tmp, k[3]);
        tmp = vaesmcq_u8(tmp);
        tmp = vaeseq_u8(tmp, k[4]);
        tmp = vaesmcq_u8(tmp);
        tmp = vaeseq_u8(tmp, k[5]);
        tmp = vaesmcq_u8(tmp);
        tmp = vaeseq_u8(tmp, k[6]);
        tmp = vaesmcq_u8(tmp);
        tmp = vaeseq_u8(tmp, k[7]);
        tmp = vaesmcq_u8(tmp);
        tmp = vaeseq_u8(tmp, k[8]);
        tmp = vaesmcq_u8(tmp);
        tmp = veorq_u8(tmp,  k[9]);
        vst1q_u8(out + i * AES_BLOCK_SIZE, tmp);
    }
}

    #ifdef FORCE_USE_HEAP
STATIC INLINE void* aligned_malloc(size_t size, size_t align)
{
    void *result;
      #ifdef _MSC_VER
    result = _aligned_malloc(size, align);
      #else /* _MSC_VER */
    if (posix_memalign(&result, align, size))
    {
        result = NULL;
    }
      #endif /* _MSC_VER */

    return result;
}

STATIC INLINE void aligned_free(void *ptr)
{
      #ifdef _MSC_VER
    _aligned_free(ptr);
      #else /*_MSC_VER */
    free(ptr);
      #endif /* _MSC_VER */
}
    #endif /* FORCE_USE_HEAP */

void cn_slow_hash(const void *data, size_t length, char *hash, int light, int variant, int prehashed, uint32_t page_size, uint32_t scratchpad, uint32_t iterations)
{
    uint32_t TOTALBLOCKS = (page_size / AES_BLOCK_SIZE);
    uint32_t init_rounds = (scratchpad / INIT_SIZE_BYTE);
    uint32_t aes_rounds = (iterations / 2);
    size_t lightFlag = (light ? 2: 1);

    RDATA_ALIGN16 uint8_t expandedKey[240];

    #ifndef FORCE_USE_HEAP
    RDATA_ALIGN16 uint8_t hp_state[page_size];
    #else /* FORCE_USE_HEAP */
      #pragma message ("warning: ACTIVATING FORCE_USE_HEAP IN aarch64 + crypto in slow-hash-arm.c")
    uint8_t *hp_state = (uint8_t *)aligned_malloc(page_size,16);
    #endif /* FORCE_USE_HEAP */

    uint8_t text[INIT_SIZE_BYTE];
    RDATA_ALIGN16 uint64_t a[2];
    RDATA_ALIGN16 uint64_t b[4];
    RDATA_ALIGN16 uint64_t c[2];
    union cn_slow_hash_state state;
    uint8x16_t _a, _b, _b1, _c, zero = {0};
    uint64_t hi, lo;

    size_t i, j;
    uint64_t *p = NULL;

    static void (*const extra_hashes[4])(const void *, size_t, char *) =
    {
        hash_extra_blake, hash_extra_groestl, hash_extra_jh, hash_extra_skein
    };

    /* CryptoNight Step 1:  Use Keccak1600 to initialize the 'state' (and 'text') buffers from the data. */

    if (prehashed)
    {
        memcpy(&state.hs, data, length);
    }
    else
    {
        hash_process(&state.hs, data, length);
    }

    memcpy(text, state.init, INIT_SIZE_BYTE);

    VARIANT1_INIT64();
    VARIANT2_INIT64();

    /* CryptoNight Step 2:  Iteratively encrypt the results from Keccak to fill
     * the 2MB large random access buffer.
     */

    aes_expand_key(state.hs.b, expandedKey);
    for(i = 0; i < init_rounds; i++)
    {
        aes_pseudo_round(text, text, expandedKey, INIT_SIZE_BLK);
        memcpy(&hp_state[i * INIT_SIZE_BYTE], text, INIT_SIZE_BYTE);
    }

    U64(a)[0] = U64(&state.k[0])[0] ^ U64(&state.k[32])[0];
    U64(a)[1] = U64(&state.k[0])[1] ^ U64(&state.k[32])[1];
    U64(b)[0] = U64(&state.k[16])[0] ^ U64(&state.k[48])[0];
    U64(b)[1] = U64(&state.k[16])[1] ^ U64(&state.k[48])[1];

    /* CryptoNight Step 3:  Bounce randomly 1,048,576 times (1<<20) through the mixing buffer,
     * using 524,288 iterations of the following mixing function.  Each execution
     * performs two reads and writes from the mixing buffer.
     */

    _b = vld1q_u8((const uint8_t *)b);
    _b1 = vld1q_u8(((const uint8_t *)b) + AES_BLOCK_SIZE);

    for(i = 0; i < aes_rounds; i++)
    {
        pre_aes();
        _c = vaeseq_u8(_c, zero);
        _c = vaesmcq_u8(_c);
        _c = veorq_u8(_c, _a);
        post_aes();
    }

    /* CryptoNight Step 4:  Sequentially pass through the mixing buffer and use 10 rounds
     * of AES encryption to mix the random data back into the 'text' buffer.  'text'
     * was originally created with the output of Keccak1600. */

    memcpy(text, state.init, INIT_SIZE_BYTE);

    aes_expand_key(&state.hs.b[32], expandedKey);
    for(i = 0; i < init_rounds; i++)
    {
        // add the xor to the pseudo round
        aes_pseudo_round_xor(text, text, expandedKey, &hp_state[i * INIT_SIZE_BYTE], INIT_SIZE_BLK);
    }

    /* CryptoNight Step 5:  Apply Keccak to the state again, and then
     * use the resulting data to select which of four finalizer
     * hash functions to apply to the data (Blake, Groestl, JH, or Skein).
     * Use this hash to squeeze the state array down
     * to the final 256 bit hash output.
     */

    memcpy(state.init, text, INIT_SIZE_BYTE);
    hash_permutation(&state.hs);
    extra_hashes[state.hs.b[0] & 3](&state, 200, hash);

    #ifdef FORCE_USE_HEAP
    aligned_free(hp_state);
    #endif /*FORCE_USE_HEAP */
}

  #else /* defined(__aarch64__) && defined(__ARM_FEATURE_CRYPTO) */

// ND: Some minor optimizations for ARMv7 (raspberrry pi 2), effect seems to be ~40-50% faster.
//     Needs more work.

    #ifdef NO_OPTIMIZED_MULTIPLY_ON_ARM
/* The asm corresponds to this C code */
      #define SHORT uint32_t
      #define LONG uint64_t

void mul(const uint8_t *ca, const uint8_t *cb, uint8_t *cres)
{
    const SHORT *aa = (SHORT *)ca;
    const SHORT *bb = (SHORT *)cb;
    SHORT *res = (SHORT *)cres;

    union
    {
        SHORT tmp[8];
        LONG ltmp[4];
    } t;

    LONG A = aa[1];
    LONG a = aa[0];
    LONG B = bb[1];
    LONG b = bb[0];

    // Aa * Bb = ab + aB_ + Ab_ + AB__
    t.ltmp[0] = a * b;
    t.ltmp[1] = a * B;
    t.ltmp[2] = A * b;
    t.ltmp[3] = A * B;

    res[2] = t.tmp[0];
    t.ltmp[1] += t.tmp[1];
    t.ltmp[1] += t.tmp[4];
    t.ltmp[3] += t.tmp[3];
    t.ltmp[3] += t.tmp[5];
    res[3] = t.tmp[2];
    res[0] = t.tmp[6];
    res[1] = t.tmp[7];
}
    #else // !NO_OPTIMIZED_MULTIPLY_ON_ARM

      #ifdef __aarch64__ /* ARM64, no crypto */
        #define mul(a, b, c)	cn_mul128((const uint64_t *)a, (const uint64_t *)b, (uint64_t *)c)
STATIC void cn_mul128(const uint64_t *a, const uint64_t *b, uint64_t *r)
{
    uint64_t lo, hi;
    __asm__("mul %0, %1, %2\n\t" : "=r"(lo) : "r"(a[0]), "r"(b[0]) );
    __asm__("umulh %0, %1, %2\n\t" : "=r"(hi) : "r"(a[0]), "r"(b[0]) );
    r[0] = hi;
    r[1] = lo;
}
      #else /* ARM32 */
        #define mul(a, b, c)	cn_mul128((const uint32_t *)a, (const uint32_t *)b, (uint32_t *)c)
STATIC void cn_mul128(const uint32_t *aa, const uint32_t *bb, uint32_t *r)
{
    uint32_t t0, t1, t2=0, t3=0;

    __asm__ __volatile__(
        "umull %[t0], %[t1], %[a], %[b]\n\t"
        "str   %[t0], %[ll]\n\t"

        // accumulating with 0 can never overflow/carry
        "eor   %[t0], %[t0]\n\t"
        "umlal %[t1], %[t0], %[a], %[B]\n\t"

        "umlal %[t1], %[t2], %[A], %[b]\n\t"
        "str   %[t1], %[lh]\n\t"

        "umlal %[t0], %[t3], %[A], %[B]\n\t"

        // final add may have a carry
        "adds  %[t0], %[t0], %[t2]\n\t"
        "adc   %[t1], %[t3], #0\n\t"

        "str   %[t0], %[hl]\n\t"
        "str   %[t1], %[hh]\n\t"
        : [t0]"=&r"(t0),
        [t1]"=&r"(t1),
        [t2]"+r"(t2),
        [t3]"+r"(t3),
        [hl]"=m"(r[0]),
        [hh]"=m"(r[1]),
        [ll]"=m"(r[2]),
        [lh]"=m"(r[3]) : [A]"r"(aa[1]),
        [a]"r"(aa[0]),
        [B]"r"(bb[1]),
        [b]"r"(bb[0]) : "cc"
    );
}
      #endif /* !aarch64 */
    #endif // NO_OPTIMIZED_MULTIPLY_ON_ARM

STATIC INLINE void copy_block(uint8_t* dst, const uint8_t* src)
{
    memcpy(dst, src, AES_BLOCK_SIZE);
}

STATIC INLINE void sum_half_blocks(uint8_t* a, const uint8_t* b)
{
    uint64_t a0, a1, b0, b1;
    a0 = U64(a)[0];
    a1 = U64(a)[1];
    b0 = U64(b)[0];
    b1 = U64(b)[1];
    a0 += b0;
    a1 += b1;
    U64(a)[0] = a0;
    U64(a)[1] = a1;
}

STATIC INLINE void swap_blocks(uint8_t *a, uint8_t *b)
{
    uint64_t t[2];
    U64(t)[0] = U64(a)[0];
    U64(t)[1] = U64(a)[1];
    U64(a)[0] = U64(b)[0];
    U64(a)[1] = U64(b)[1];
    U64(b)[0] = U64(t)[0];
    U64(b)[1] = U64(t)[1];
}

STATIC INLINE void xor_blocks(uint8_t* a, const uint8_t* b)
{
    U64(a)[0] ^= U64(b)[0];
    U64(a)[1] ^= U64(b)[1];
}

void cn_slow_hash(const void *data, size_t length, char *hash, int light, int variant, int prehashed, uint32_t page_size, uint32_t scratchpad, uint32_t iterations)
{
    uint32_t init_rounds = (scratchpad / INIT_SIZE_BYTE);
    uint32_t aes_rounds = (iterations / 2);
    size_t lightFlag = (light ? 2: 1);

    uint8_t text[INIT_SIZE_BYTE];
    uint8_t a[AES_BLOCK_SIZE];
    uint8_t b[AES_BLOCK_SIZE * 2];
    uint8_t c[AES_BLOCK_SIZE];
    uint8_t c1[AES_BLOCK_SIZE];
    uint8_t d[AES_BLOCK_SIZE];
    uint8_t aes_key[AES_KEY_SIZE];
    RDATA_ALIGN16 uint8_t expandedKey[256];

    union cn_slow_hash_state state;

    size_t i, j;
    uint8_t *p = NULL;
    oaes_ctx *aes_ctx;

    static void (*const extra_hashes[4])(const void *, size_t, char *) =
    {
        hash_extra_blake, hash_extra_groestl, hash_extra_jh, hash_extra_skein
    };

  #ifndef FORCE_USE_HEAP
    uint8_t long_state[page_size];
  #else /* FORCE_USE_HEAP */
    #pragma message ("warning: ACTIVATING FORCE_USE_HEAP IN aarch64 && !crypto in slow-hash.c")
    uint8_t *long_state = (uint8_t *)malloc(page_size);
  #endif /* FORCE_USE_HEAP */

    if (prehashed)
    {
        memcpy(&state.hs, data, length);
    } else {
        hash_process(&state.hs, data, length);
    }

    memcpy(text, state.init, INIT_SIZE_BYTE);

    aes_ctx = (oaes_ctx *) oaes_alloc();
    oaes_key_import_data(aes_ctx, state.hs.b, AES_KEY_SIZE);

    VARIANT1_INIT64();
    VARIANT2_INIT64();

    // use aligned data
    memcpy(expandedKey, aes_ctx->key->exp_data, aes_ctx->key->exp_data_len);

    for(i = 0; i < init_rounds; i++)
    {
        for(j = 0; j < INIT_SIZE_BLK; j++)
            aesb_pseudo_round(&text[AES_BLOCK_SIZE * j], &text[AES_BLOCK_SIZE * j], expandedKey);
        memcpy(&long_state[i * INIT_SIZE_BYTE], text, INIT_SIZE_BYTE);
    }

    U64(a)[0] = U64(&state.k[0])[0] ^ U64(&state.k[32])[0];
    U64(a)[1] = U64(&state.k[0])[1] ^ U64(&state.k[32])[1];
    U64(b)[0] = U64(&state.k[16])[0] ^ U64(&state.k[48])[0];
    U64(b)[1] = U64(&state.k[16])[1] ^ U64(&state.k[48])[1];

    for(i = 0; i < aes_rounds; i++)
    {
    #define MASK(div) ((uint32_t)(((page_size / AES_BLOCK_SIZE) / (div) - 1) << 4))
    #define state_index(x,div) ((*(uint32_t *) x) & MASK(div))

      // Iteration 1
      j = state_index(a,lightFlag);
      p = &long_state[j];
      aesb_single_round(p, p, a);
      copy_block(c1, p);

      VARIANT2_PORTABLE_SHUFFLE_ADD(long_state, j);
      xor_blocks(p, b);
      VARIANT1_1(p);

      // Iteration 2
      j = state_index(c1,lightFlag);
      p = &long_state[j];
      copy_block(c, p);

      VARIANT2_PORTABLE_INTEGER_MATH(c, c1);
      mul(c1, c, d);
      VARIANT2_2_PORTABLE();
      VARIANT2_PORTABLE_SHUFFLE_ADD(long_state, j);
      sum_half_blocks(a, d);
      swap_blocks(a, c);
      xor_blocks(a, c);
      VARIANT1_2(U64(c) + 1);
      copy_block(p, c);

      if (variant >= 2) {
          copy_block(b + AES_BLOCK_SIZE, b);
      }

      copy_block(b, c1);
    }

    memcpy(text, state.init, INIT_SIZE_BYTE);
    oaes_key_import_data(aes_ctx, &state.hs.b[32], AES_KEY_SIZE);
    memcpy(expandedKey, aes_ctx->key->exp_data, aes_ctx->key->exp_data_len);

    for(i = 0; i < init_rounds; i++)
    {
        for(j = 0; j < INIT_SIZE_BLK; j++)
        {
            xor_blocks(&text[j * AES_BLOCK_SIZE], &long_state[i * INIT_SIZE_BYTE + j * AES_BLOCK_SIZE]);
            aesb_pseudo_round(&text[AES_BLOCK_SIZE * j], &text[AES_BLOCK_SIZE * j], expandedKey);
        }
    }

    oaes_free((OAES_CTX **) &aes_ctx);
    memcpy(state.init, text, INIT_SIZE_BYTE);
    hash_permutation(&state.hs);
    extra_hashes[state.hs.b[0] & 3](&state, 200, hash);

    #ifdef FORCE_USE_HEAP
    free(long_state);
    #endif /* FORCE_USE_HEAP */
}

  #endif /* defined(__aarch64__) && defined(__ARM_FEATURE_CRYPTO) */

#endif
