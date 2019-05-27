// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2014-2018, The Aeon Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

/* This file contains the x86 version of the CryptoNight slow-hash routines */

#if !defined NO_AES && (defined(__x86_64__) || (defined(_MSC_VER) && defined(_WIN64)))
  #pragma message ("info: Using slow-hash-x86.c")

  #include "slow-hash-common.h"

// Optimised code below, uses x86-specific intrinsics, SSE2, AES-NI
// Fall back to more portable code is down at the bottom

  #include <emmintrin.h>

  #if defined(_MSC_VER)
    #include <intrin.h>
    #include <windows.h>
    #define STATIC
    #define INLINE __inline
    #if !defined(RDATA_ALIGN16)
      #define RDATA_ALIGN16 __declspec(align(16))
    #endif
  #elif defined(__MINGW32__)
    #include <intrin.h>
    #include <windows.h>
    #define STATIC static
    #define INLINE inline
    #if !defined(RDATA_ALIGN16)
      #define RDATA_ALIGN16 __attribute__ ((aligned(16)))
    #endif
  #else
    #include <wmmintrin.h>
    #include <sys/mman.h>
    #define STATIC static
    #define INLINE inline
    #if !defined(RDATA_ALIGN16)
      #define RDATA_ALIGN16 __attribute__ ((aligned(16)))
    #endif
  #endif

  #if defined(__INTEL_COMPILER)
    #define ASM __asm__
  #elif !defined(_MSC_VER)
    #define ASM __asm__
  #else
    #define ASM __asm
  #endif

  #define U64(x) ((uint64_t *) (x))
  #define R128(x) ((__m128i *) (x))

  #define state_index(x,div) (((*((uint64_t *)x) >> 4) & (TOTALBLOCKS /(div) - 1)) << 4)

  #if defined(_MSC_VER)
    #if !defined(_WIN64)
      #define __mul() lo = mul128(c[0], b[0], &hi);
    #else
      #define __mul() lo = _umul128(c[0], b[0], &hi);
    #endif
  #else
    #if defined(__x86_64__)
      #define __mul() ASM("mulq %3\n\t" : "=d"(hi), "=a"(lo) : "%a" (c[0]), "rm" (b[0]) : "cc");
    #else
      #define __mul() lo = mul128(c[0], b[0], &hi);
    #endif
  #endif

# define pre_aes() \
  j = state_index(a,lightFlag); \
  _c = _mm_load_si128(R128(&hp_state[j])); \
  _a = _mm_load_si128(R128(a)); \

/*
 * An SSE-optimized implementation of the second half of CryptoNight step 3.
 * After using AES to mix a scratchpad value into _c (done by the caller),
 * this macro xors it with _b and stores the result back to the same index (j) that it
 * loaded the scratchpad value from.  It then performs a second random memory
 * read/write from the scratchpad, but this time mixes the values using a 64
 * bit multiply.
 * This code is based upon an optimized implementation by dga.
 */
  #define post_aes() \
  VARIANT2_SHUFFLE_ADD_SSE2(hp_state, j); \
  _mm_store_si128(R128(c), _c); \
  _mm_store_si128(R128(&hp_state[j]), _mm_xor_si128(_b, _c)); \
  VARIANT1_1(&hp_state[j]); \
  j = state_index(c,lightFlag); \
  p = U64(&hp_state[j]); \
  b[0] = p[0]; b[1] = p[1]; \
  VARIANT2_INTEGER_MATH_SSE2(b, c); \
  __mul(); \
  VARIANT2_2(); \
  VARIANT2_SHUFFLE_ADD_SSE2(hp_state, j); \
  a[0] += hi; a[1] += lo; \
  p = U64(&hp_state[j]); \
  p[0] = a[0];  p[1] = a[1]; \
  a[0] ^= b[0]; a[1] ^= b[1]; \
  VARIANT1_2(p + 1); \
  _b1 = _b; \
  _b = _c; \

  #if defined(_MSC_VER)
    #define THREADV __declspec(thread)
  #else
    #define THREADV __thread
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

THREADV uint8_t *hp_state = NULL;
THREADV int hp_allocated = 0;

  #if defined(_MSC_VER)
    #define cpuid(info,x)    __cpuidex(info,x,0)
  #else
void cpuid(int CPUInfo[4], int InfoType)
{
    ASM __volatile__
    (
      "cpuid":
          "=a" (CPUInfo[0]),
          "=b" (CPUInfo[1]),
          "=c" (CPUInfo[2]),
          "=d" (CPUInfo[3]) : "a" (InfoType),
          "c" (0)
    );
}
  #endif

/**
 * @brief a = (a xor b), where a and b point to 128 bit values
 */

STATIC INLINE void xor_blocks(uint8_t *a, const uint8_t *b)
{
    U64(a)[0] ^= U64(b)[0];
    U64(a)[1] ^= U64(b)[1];
}

STATIC INLINE void xor64(uint64_t *a, const uint64_t b)
{
    *a ^= b;
}

/**
 * @brief uses cpuid to determine if the CPU supports the AES instructions
 * @return true if the CPU supports AES, false otherwise
 */

STATIC INLINE int force_software_aes(void)
{
    static int use = -1;

    if (use != -1)
    {
        return use;
    }

    const char *env = getenv("TURTLECOIN_USE_SOFTWARE_AES");
    if (!env)
    {
        use = 0;
    }
    else if (!strcmp(env, "0") || !strcmp(env, "no"))
    {
        use = 0;
    }
    else
    {
        use = 1;
    }

    return use;
}

STATIC INLINE int check_aes_hw(void)
{
    int cpuid_results[4];
    static int supported = -1;

    if(supported >= 0)
    {
        return supported;
    }

    cpuid(cpuid_results,1);

    return supported = cpuid_results[2] & (1 << 25);
}

STATIC INLINE void aes_256_assist1(__m128i* t1, __m128i * t2)
{
    __m128i t4;
    *t2 = _mm_shuffle_epi32(*t2, 0xff);
    t4 = _mm_slli_si128(*t1, 0x04);
    *t1 = _mm_xor_si128(*t1, t4);
    t4 = _mm_slli_si128(t4, 0x04);
    *t1 = _mm_xor_si128(*t1, t4);
    t4 = _mm_slli_si128(t4, 0x04);
    *t1 = _mm_xor_si128(*t1, t4);
    *t1 = _mm_xor_si128(*t1, *t2);
}

STATIC INLINE void aes_256_assist2(__m128i* t1, __m128i * t3)
{
    __m128i t2, t4;
    t4 = _mm_aeskeygenassist_si128(*t1, 0x00);
    t2 = _mm_shuffle_epi32(t4, 0xaa);
    t4 = _mm_slli_si128(*t3, 0x04);
    *t3 = _mm_xor_si128(*t3, t4);
    t4 = _mm_slli_si128(t4, 0x04);
    *t3 = _mm_xor_si128(*t3, t4);
    t4 = _mm_slli_si128(t4, 0x04);
    *t3 = _mm_xor_si128(*t3, t4);
    *t3 = _mm_xor_si128(*t3, t2);
}

/**
 * @brief expands 'key' into a form it can be used for AES encryption.
 *
 * This is an SSE-optimized implementation of AES key schedule generation.  It
 * expands the key into multiple round keys, each of which is used in one round
 * of the AES encryption used to fill (and later, extract randomness from)
 * the large 2MB buffer.  Note that CryptoNight does not use a completely
 * standard AES encryption for its buffer expansion, so do not copy this
 * function outside of Monero without caution!  This version uses the hardware
 * AESKEYGENASSIST instruction to speed key generation, and thus requires
 * CPU AES support.
 * For more information about these functions, see page 19 of Intel's AES instructions
 * white paper:
 * https://www.intel.com/content/dam/doc/white-paper/advanced-encryption-standard-new-instructions-set-paper.pdf
 *
 * @param key the input 128 bit key
 * @param expandedKey An output buffer to hold the generated key schedule
 */

STATIC INLINE void aes_expand_key(const uint8_t *key, uint8_t *expandedKey)
{
    __m128i *ek = R128(expandedKey);
    __m128i t1, t2, t3;

    t1 = _mm_loadu_si128(R128(key));
    t3 = _mm_loadu_si128(R128(key + 16));

    ek[0] = t1;
    ek[1] = t3;

    t2 = _mm_aeskeygenassist_si128(t3, 0x01);
    aes_256_assist1(&t1, &t2);
    ek[2] = t1;
    aes_256_assist2(&t1, &t3);
    ek[3] = t3;

    t2 = _mm_aeskeygenassist_si128(t3, 0x02);
    aes_256_assist1(&t1, &t2);
    ek[4] = t1;
    aes_256_assist2(&t1, &t3);
    ek[5] = t3;

    t2 = _mm_aeskeygenassist_si128(t3, 0x04);
    aes_256_assist1(&t1, &t2);
    ek[6] = t1;
    aes_256_assist2(&t1, &t3);
    ek[7] = t3;

    t2 = _mm_aeskeygenassist_si128(t3, 0x08);
    aes_256_assist1(&t1, &t2);
    ek[8] = t1;
    aes_256_assist2(&t1, &t3);
    ek[9] = t3;

    t2 = _mm_aeskeygenassist_si128(t3, 0x10);
    aes_256_assist1(&t1, &t2);
    ek[10] = t1;
}

/**
 * @brief a "pseudo" round of AES (similar to but slightly different from normal AES encryption)
 *
 * To fill its 2MB scratch buffer, CryptoNight uses a nonstandard implementation
 * of AES encryption:  It applies 10 rounds of the basic AES encryption operation
 * to an input 128 bit chunk of data <in>.  Unlike normal AES, however, this is
 * all it does;  it does not perform the initial AddRoundKey step (this is done
 * in subsequent steps by aesenc_si128), and it does not use the simpler final round.
 * Hence, this is a "pseudo" round - though the function actually implements 10 rounds together.
 *
 * Note that unlike aesb_pseudo_round, this function works on multiple data chunks.
 *
 * @param in a pointer to nblocks * 128 bits of data to be encrypted
 * @param out a pointer to an nblocks * 128 bit buffer where the output will be stored
 * @param expandedKey the expanded AES key
 * @param nblocks the number of 128 blocks of data to be encrypted
 */

STATIC INLINE void aes_pseudo_round(const uint8_t *in, uint8_t *out, const uint8_t *expandedKey, int nblocks)
{
    __m128i *k = R128(expandedKey);
    __m128i d;
    int i;

    for(i = 0; i < nblocks; i++)
    {
        d = _mm_loadu_si128(R128(in + i * AES_BLOCK_SIZE));
        d = _mm_aesenc_si128(d, *R128(&k[0]));
        d = _mm_aesenc_si128(d, *R128(&k[1]));
        d = _mm_aesenc_si128(d, *R128(&k[2]));
        d = _mm_aesenc_si128(d, *R128(&k[3]));
        d = _mm_aesenc_si128(d, *R128(&k[4]));
        d = _mm_aesenc_si128(d, *R128(&k[5]));
        d = _mm_aesenc_si128(d, *R128(&k[6]));
        d = _mm_aesenc_si128(d, *R128(&k[7]));
        d = _mm_aesenc_si128(d, *R128(&k[8]));
        d = _mm_aesenc_si128(d, *R128(&k[9]));
        _mm_storeu_si128((R128(out + i * AES_BLOCK_SIZE)), d);
    }
}

/**
 * @brief aes_pseudo_round that loads data from *in and xors it with *xor first
 *
 * This function performs the same operations as aes_pseudo_round, but before
 * performing the encryption of each 128 bit block from <in>, it xors
 * it with the corresponding block from <xor>.
 *
 * @param in a pointer to nblocks * 128 bits of data to be encrypted
 * @param out a pointer to an nblocks * 128 bit buffer where the output will be stored
 * @param expandedKey the expanded AES key
 * @param xor a pointer to an nblocks * 128 bit buffer that is xored into in before encryption (in is left unmodified)
 * @param nblocks the number of 128 blocks of data to be encrypted
 */

STATIC INLINE void aes_pseudo_round_xor(const uint8_t *in, uint8_t *out, const uint8_t *expandedKey, const uint8_t *xor, int nblocks)
{
    __m128i *k = R128(expandedKey);
    __m128i *x = R128(xor);
    __m128i d;
    int i;

    for(i = 0; i < nblocks; i++)
    {
        d = _mm_loadu_si128(R128(in + i * AES_BLOCK_SIZE));
        d = _mm_xor_si128(d, *R128(x++));
        d = _mm_aesenc_si128(d, *R128(&k[0]));
        d = _mm_aesenc_si128(d, *R128(&k[1]));
        d = _mm_aesenc_si128(d, *R128(&k[2]));
        d = _mm_aesenc_si128(d, *R128(&k[3]));
        d = _mm_aesenc_si128(d, *R128(&k[4]));
        d = _mm_aesenc_si128(d, *R128(&k[5]));
        d = _mm_aesenc_si128(d, *R128(&k[6]));
        d = _mm_aesenc_si128(d, *R128(&k[7]));
        d = _mm_aesenc_si128(d, *R128(&k[8]));
        d = _mm_aesenc_si128(d, *R128(&k[9]));
        _mm_storeu_si128((R128(out + i * AES_BLOCK_SIZE)), d);
    }
}

  #if defined(_MSC_VER) || defined(__MINGW32__)
BOOL SetLockPagesPrivilege(HANDLE hProcess, BOOL bEnable)
{
    struct
    {
        DWORD count;
        LUID_AND_ATTRIBUTES privilege[1];
    } info;

    HANDLE token;

    if(!OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES, &token))
    {
        return FALSE;
    }

    info.count = 1;
    info.privilege[0].Attributes = bEnable ? SE_PRIVILEGE_ENABLED : 0;

    if(!LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &(info.privilege[0].Luid)))
    {
        return FALSE;
    }

    if(!AdjustTokenPrivileges(token, FALSE, (PTOKEN_PRIVILEGES) &info, 0, NULL, NULL))
    {
        return FALSE;
    }

    if (GetLastError() != ERROR_SUCCESS)
    {
        return FALSE;
    }

    CloseHandle(token);

    return TRUE;
}
  #endif

/**
 * @brief allocate the 2MB scratch buffer using OS support for huge pages, if available
 *
 * This function tries to allocate the 2MB scratch buffer using a single
 * 2MB "huge page" (instead of the usual 4KB page sizes) to reduce TLB misses
 * during the random accesses to the scratch buffer.  This is one of the
 * important speed optimizations needed to make CryptoNight faster.
 *
 * No parameters.  Updates a thread-local pointer, hp_state, to point to
 * the allocated buffer.
 */

void slow_hash_allocate_state(uint32_t page_size)
{
    if(hp_state != NULL)
    {
        return;
    }

  #if defined(_MSC_VER) || defined(__MINGW32__)
    SetLockPagesPrivilege(GetCurrentProcess(), TRUE);
    hp_state = (uint8_t *) VirtualAlloc(hp_state, page_size, MEM_LARGE_PAGES | MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  #else
    #if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__) || defined(__NetBSD__)
    hp_state = mmap(0, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 0, 0);
    #else
    hp_state = mmap(0, page_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0);
    #endif

    if(hp_state == MAP_FAILED)
    {
        hp_state = NULL;
    }
  #endif

    hp_allocated = 1;

    if(hp_state == NULL)
    {
        hp_allocated = 0;
        hp_state = (uint8_t *) malloc(page_size);
    }
}

/**
 *@brief frees the state allocated by slow_hash_allocate_state
 */

void slow_hash_free_state(uint32_t page_size)
{
    if(hp_state == NULL)
    {
        return;
    }

    if(!hp_allocated)
    {
        free(hp_state);
    }
    else
    {
      #if defined(_MSC_VER) || defined(__MINGW32__)
        VirtualFree(hp_state, 0, MEM_RELEASE);
      #else
        munmap(hp_state, page_size);
      #endif
    }

    hp_state = NULL;
    hp_allocated = 0;
}

/**
 * @brief the hash function implementing CryptoNight, used for the Monero proof-of-work
 *
 * Computes the hash of <data> (which consists of <length> bytes), returning the
 * hash in <hash>.  The CryptoNight hash operates by first using Keccak 1600,
 * the 1600 bit variant of the Keccak hash used in SHA-3, to create a 200 byte
 * buffer of pseudorandom data by hashing the supplied data.  It then uses this
 * random data to fill a large 2MB buffer with pseudorandom data by iteratively
 * encrypting it using 10 rounds of AES per entry.  After this initialization,
 * it executes 524,288 rounds of mixing through the random 2MB buffer using
 * AES (typically provided in hardware on modern CPUs) and a 64 bit multiply.
 * Finally, it re-mixes this large buffer back into
 * the 200 byte "text" buffer, and then hashes this buffer using one of four
 * pseudorandomly selected hash functions (Blake, Groestl, JH, or Skein)
 * to populate the output.
 *
 * The 2MB buffer and choice of functions for mixing are designed to make the
 * algorithm "CPU-friendly" (and thus, reduce the advantage of GPU, FPGA,
 * or ASIC-based implementations):  the functions used are fast on modern
 * CPUs, and the 2MB size matches the typical amount of L3 cache available per
 * core on 2013-era CPUs.  When available, this implementation will use hardware
 * AES support on x86 CPUs.
 *
 * A diagram of the inner loop of this function can be found at
 * https://www.cs.cmu.edu/~dga/crypto/xmr/cryptonight.png
 *
 * @param data the data to hash
 * @param length the length in bytes of the data
 * @param hash a pointer to a buffer in which the final 256 bit hash will be stored
 */
void cn_slow_hash(const void *data, size_t length, char *hash, int light, int variant, int prehashed, uint32_t page_size, uint32_t scratchpad, uint32_t iterations)
{
    uint32_t TOTALBLOCKS = (page_size / AES_BLOCK_SIZE);
    uint32_t init_rounds = (scratchpad / INIT_SIZE_BYTE);
    uint32_t aes_rounds = (iterations / 2);
    size_t lightFlag = (light ? 2: 1);

    RDATA_ALIGN16 uint8_t expandedKey[240];  /* These buffers are aligned to use later with SSE functions */

    uint8_t text[INIT_SIZE_BYTE];
    RDATA_ALIGN16 uint64_t a[2];
    RDATA_ALIGN16 uint64_t b[4];
    RDATA_ALIGN16 uint64_t c[2];
    union cn_slow_hash_state state;
    __m128i _a, _b, _b1, _c;
    uint64_t hi, lo;

    size_t i, j;
    uint64_t *p = NULL;
    oaes_ctx *aes_ctx = NULL;
    int useAes = !force_software_aes() && check_aes_hw();

    static void (*const extra_hashes[4])(const void *, size_t, char *) =
    {
        hash_extra_blake, hash_extra_groestl, hash_extra_jh, hash_extra_skein
    };

    slow_hash_allocate_state(page_size);

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
    if(useAes)
    {
        aes_expand_key(state.hs.b, expandedKey);

        for(i = 0; i < init_rounds; i++)
        {
            aes_pseudo_round(text, text, expandedKey, INIT_SIZE_BLK);
            memcpy(&hp_state[i * INIT_SIZE_BYTE], text, INIT_SIZE_BYTE);
        }
    }
    else
    {
        aes_ctx = (oaes_ctx *) oaes_alloc();
        oaes_key_import_data(aes_ctx, state.hs.b, AES_KEY_SIZE);

        for(i = 0; i < init_rounds; i++)
        {
            for(j = 0; j < INIT_SIZE_BLK; j++)
            {
                aesb_pseudo_round(&text[AES_BLOCK_SIZE * j], &text[AES_BLOCK_SIZE * j], aes_ctx->key->exp_data);
            }

            memcpy(&hp_state[i * INIT_SIZE_BYTE], text, INIT_SIZE_BYTE);
        }
    }

    U64(a)[0] = U64(&state.k[0])[0] ^ U64(&state.k[32])[0];
    U64(a)[1] = U64(&state.k[0])[1] ^ U64(&state.k[32])[1];
    U64(b)[0] = U64(&state.k[16])[0] ^ U64(&state.k[48])[0];
    U64(b)[1] = U64(&state.k[16])[1] ^ U64(&state.k[48])[1];

    /* CryptoNight Step 3:  Bounce randomly 1,048,576 times (1<<20) through the mixing buffer,
     * using 524,288 iterations of the following mixing function.  Each execution
     * performs two reads and writes from the mixing buffer.
     */

    _b = _mm_load_si128(R128(b));
    _b1 = _mm_load_si128(R128(b) + 1);

    // Two independent versions, one with AES, one without, to ensure that
    // the useAes test is only performed once, not every iteration.
    if(useAes)
    {
        for(i = 0; i < aes_rounds; i++)
        {
            pre_aes();
            _c = _mm_aesenc_si128(_c, _a);
            post_aes();
        }
    }
    else
    {
        for(i = 0; i < aes_rounds; i++)
        {
            pre_aes();
            aesb_single_round((uint8_t *) &_c, (uint8_t *) &_c, (uint8_t *) &_a);
            post_aes();
        }
    }

    /* CryptoNight Step 4:  Sequentially pass through the mixing buffer and use 10 rounds
     * of AES encryption to mix the random data back into the 'text' buffer.  'text'
     * was originally created with the output of Keccak1600. */

    memcpy(text, state.init, INIT_SIZE_BYTE);
    if(useAes)
    {
        aes_expand_key(&state.hs.b[32], expandedKey);

        for(i = 0; i < init_rounds; i++)
        {
            // add the xor to the pseudo round
            aes_pseudo_round_xor(text, text, expandedKey, &hp_state[i * INIT_SIZE_BYTE], INIT_SIZE_BLK);
        }
    }
    else
    {
        oaes_key_import_data(aes_ctx, &state.hs.b[32], AES_KEY_SIZE);
        for(i = 0; i < init_rounds; i++)
        {
            for(j = 0; j < INIT_SIZE_BLK; j++)
            {
                xor_blocks(&text[j * AES_BLOCK_SIZE], &hp_state[i * INIT_SIZE_BYTE + j * AES_BLOCK_SIZE]);
                aesb_pseudo_round(&text[AES_BLOCK_SIZE * j], &text[AES_BLOCK_SIZE * j], aes_ctx->key->exp_data);
            }
        }

        oaes_free((OAES_CTX **) &aes_ctx);
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
    slow_hash_free_state(page_size);
}

#endif
