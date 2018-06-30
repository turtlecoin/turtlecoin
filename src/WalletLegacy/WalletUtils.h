// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <exception>
#include <iomanip>
#include <iostream>

#include "IWalletLegacy.h"
#include "Wallet/WalletErrors.h"

namespace CryptoNote
{

    inline void throwIf(bool expr, CryptoNote::error::WalletErrorCodes ec)
    {
        if (expr)
            throw std::system_error(make_error_code(ec));
    }

    inline std::ostream &operator<<(std::ostream &ostr, const Crypto::Hash &hash)
    {
        std::ios_base::fmtflags flags = ostr.setf(std::ios_base::hex, std::ios_base::basefield);
        char fill = ostr.fill('0');

        for (auto b : hash.data)
        {
            ostr << std::setw(2) << static_cast<unsigned int>(b);
        }

        ostr.fill(fill);
        ostr.setf(flags);
        return ostr;
    }

} //namespace CryptoNote
