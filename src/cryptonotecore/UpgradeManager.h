// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IUpgradeDetector.h"
#include "IUpgradeManager.h"

#include <memory>

namespace CryptoNote
{
    // Simple upgrade manager version. It doesn't support voting for now.
    class UpgradeManager : public IUpgradeManager
    {
      public:
        UpgradeManager();

        virtual ~UpgradeManager();

        virtual void addMajorBlockVersion(uint8_t targetVersion, uint32_t upgradeHeight) override;

        virtual uint8_t getBlockMajorVersion(uint32_t blockIndex) const override;

      private:
        std::vector<std::unique_ptr<IUpgradeDetector>> m_upgradeDetectors;
    };

} // namespace CryptoNote
