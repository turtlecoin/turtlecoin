// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <sstream>

namespace CryptoNote
{
    class IStreamSerializable
    {
      public:
        virtual void save(std::ostream &os) = 0;

        virtual void load(std::istream &in) = 0;
    };

} // namespace CryptoNote
