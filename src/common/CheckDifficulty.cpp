// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

////////////////////////////
#include "CheckDifficulty.h"
////////////////////////////

#include <common/int-util.h>
#include <crypto/hash.h>

namespace CryptoNote
{
#if defined(__SIZEOF_INT128__)

    static inline void mul(uint64_t a, uint64_t b, uint64_t &low, uint64_t &high)
    {
        typedef unsigned __int128 uint128_t;
        uint128_t res = (uint128_t)a * (uint128_t)b;
        low = (uint64_t)res;
        high = (uint64_t)(res >> 64);
    }

#else

    static inline void mul(uint64_t a, uint64_t b, uint64_t &low, uint64_t &high)
    {
        low = mul128(a, b, &high);
    }

#endif

    static inline bool cadd(uint64_t a, uint64_t b)
    {
        return a + b < a;
    }

    static inline bool cadc(uint64_t a, uint64_t b, bool c)
    {
        return a + b < a || (c && a + b == (uint64_t)-1);
    }

    bool check_hash(const Crypto::Hash &hash, uint64_t difficulty)
    {
        uint64_t low, high, top, cur;
        // First check the highest word, this will most likely fail for a random hash.
        mul(swap64le(((const uint64_t *)&hash)[3]), difficulty, top, high);

        if (high != 0)
        {
            return false;
        }

        mul(swap64le(((const uint64_t *)&hash)[0]), difficulty, low, cur);
        mul(swap64le(((const uint64_t *)&hash)[1]), difficulty, low, high);

        bool carry = cadd(cur, low);

        cur = high;

        mul(swap64le(((const uint64_t *)&hash)[2]), difficulty, low, high);

        carry = cadc(cur, low, carry);
        carry = cadc(high, top, carry);

        return !carry;
    }
} // namespace CryptoNote
