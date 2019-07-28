// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>

namespace CryptoNote
{
    class IUpgradeManager
    {
      public:
        virtual ~IUpgradeManager() {}

        virtual void addMajorBlockVersion(uint8_t targetVersion, uint32_t upgradeHeight) = 0;

        virtual uint8_t getBlockMajorVersion(uint32_t blockIndex) const = 0;
    };

} // namespace CryptoNote
