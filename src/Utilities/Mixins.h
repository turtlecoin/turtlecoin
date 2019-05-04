// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <string>

#include <system_error>

namespace Utilities
{
    /* Returns {minMixin, maxMixin, defaultMixin} */
    std::tuple<uint64_t, uint64_t, uint64_t> getMixinAllowableRange(const uint64_t height);

    /* This method is used by WalletService to determine if the mixin amount is correct
     for the current block height */
    std::tuple<bool, std::string, std::error_code> validate(
        const uint64_t mixin,
        const uint64_t height);
}
