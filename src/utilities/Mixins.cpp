// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

/////////////////////////////
#include <utilities/Mixins.h>
/////////////////////////////

#include <config/CryptoNoteConfig.h>
#include <sstream>
#include <tuple>

namespace Utilities
{
    /* Returns {minMixin, maxMixin, defaultMixin} */
    std::tuple<uint64_t, uint64_t, uint64_t> getMixinAllowableRange(const uint64_t height)
    {
        uint64_t minMixin = 0;
        uint64_t maxMixin = std::numeric_limits<uint64_t>::max();
        uint64_t defaultMixin = CryptoNote::parameters::DEFAULT_MIXIN_V0;

        /* We now limit the mixin allowed in a transaction. However, there have been
           some transactions outside these limits in the past, so we need to only
           enforce this on new blocks, otherwise wouldn't be able to sync the chain */

        /* We also need to ensure that the mixin enforced is for the limit that
           was correct when the block was formed - i.e. if 0 mixin was allowed at
           block 100, but is no longer allowed - we should still validate block 100 */

        if (height >= CryptoNote::parameters::MIXIN_LIMITS_V3_HEIGHT)
        {
            minMixin = CryptoNote::parameters::MINIMUM_MIXIN_V3;
            maxMixin = CryptoNote::parameters::MAXIMUM_MIXIN_V3;
            defaultMixin = CryptoNote::parameters::DEFAULT_MIXIN_V3;
        }
        else if (height >= CryptoNote::parameters::MIXIN_LIMITS_V2_HEIGHT)
        {
            minMixin = CryptoNote::parameters::MINIMUM_MIXIN_V2;
            maxMixin = CryptoNote::parameters::MAXIMUM_MIXIN_V2;
            defaultMixin = CryptoNote::parameters::DEFAULT_MIXIN_V2;
        }
        else if (height >= CryptoNote::parameters::MIXIN_LIMITS_V1_HEIGHT)
        {
            minMixin = CryptoNote::parameters::MINIMUM_MIXIN_V1;
            maxMixin = CryptoNote::parameters::MAXIMUM_MIXIN_V1;
            defaultMixin = CryptoNote::parameters::DEFAULT_MIXIN_V1;
        }

        return {minMixin, maxMixin, defaultMixin};
    }
} // namespace Utilities
