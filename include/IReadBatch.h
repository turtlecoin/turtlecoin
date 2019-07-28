// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <string>
#include <utility>
#include <vector>

namespace CryptoNote
{
    class IReadBatch
    {
      public:
        virtual std::vector<std::string> getRawKeys() const = 0;

        virtual void submitRawResult(const std::vector<std::string> &values, const std::vector<bool> &resultStates) = 0;
    };

} // namespace CryptoNote
