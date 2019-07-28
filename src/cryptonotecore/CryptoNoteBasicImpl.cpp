// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "CryptoNoteBasicImpl.h"

#include "CryptoNoteFormatUtils.h"
#include "common/Base58.h"
#include "common/CryptoNoteTools.h"
#include "common/int-util.h"
#include "crypto/hash.h"
#include "serialization/CryptoNoteSerialization.h"

using namespace Crypto;
using namespace Common;

namespace CryptoNote
{
    /************************************************************************/
    /* CryptoNote helper functions                                          */
    /************************************************************************/
    //-----------------------------------------------------------------------------------------------
    uint64_t getPenalizedAmount(uint64_t amount, size_t medianSize, size_t currentBlockSize)
    {
        static_assert(sizeof(size_t) >= sizeof(uint32_t), "size_t is too small");
        assert(currentBlockSize <= 2 * medianSize);
        assert(medianSize <= std::numeric_limits<uint32_t>::max());
        assert(currentBlockSize <= std::numeric_limits<uint32_t>::max());

        if (amount == 0)
        {
            return 0;
        }

        if (currentBlockSize <= medianSize)
        {
            return amount;
        }

        uint64_t productHi;
        uint64_t productLo =
            mul128(amount, currentBlockSize * (UINT64_C(2) * medianSize - currentBlockSize), &productHi);

        uint64_t penalizedAmountHi;
        uint64_t penalizedAmountLo;
        div128_32(productHi, productLo, static_cast<uint32_t>(medianSize), &penalizedAmountHi, &penalizedAmountLo);
        div128_32(
            penalizedAmountHi,
            penalizedAmountLo,
            static_cast<uint32_t>(medianSize),
            &penalizedAmountHi,
            &penalizedAmountLo);

        assert(0 == penalizedAmountHi);
        assert(penalizedAmountLo < amount);

        return penalizedAmountLo;
    }
} // namespace CryptoNote

//--------------------------------------------------------------------------------
bool parse_hash256(const std::string &str_hash, Crypto::Hash &hash)
{
    return Common::podFromHex(str_hash, hash);
}
