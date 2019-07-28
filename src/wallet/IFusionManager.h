// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace CryptoNote
{
    class IFusionManager
    {
      public:
        struct EstimateResult
        {
            size_t fusionReadyCount;
            size_t totalOutputCount;
        };

        virtual ~IFusionManager() {}

        virtual size_t createFusionTransaction(
            uint64_t threshold,
            uint16_t mixin,
            const std::vector<std::string> &sourceAddresses = {},
            const std::string &destinationAddress = "") = 0;

        virtual bool isFusionTransaction(size_t transactionId) const = 0;

        virtual EstimateResult
            estimate(uint64_t threshold, const std::vector<std::string> &sourceAddresses = {}) const = 0;
    };

} // namespace CryptoNote
