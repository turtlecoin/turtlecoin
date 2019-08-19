// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "UpgradeManager.h"

#include <config/CryptoNoteConfig.h>

namespace CryptoNote
{
    UpgradeManager::UpgradeManager() {}

    UpgradeManager::~UpgradeManager() {}

    void UpgradeManager::addMajorBlockVersion(uint8_t targetVersion, uint32_t upgradeHeight)
    {
        assert(m_upgradeDetectors.empty() || m_upgradeDetectors.back()->targetVersion() < targetVersion);
        m_upgradeDetectors.emplace_back(makeUpgradeDetector(targetVersion, upgradeHeight));
    }

    uint8_t UpgradeManager::getBlockMajorVersion(uint32_t blockIndex) const
    {
        for (auto it = m_upgradeDetectors.rbegin(); it != m_upgradeDetectors.rend(); ++it)
        {
            if (it->get()->upgradeIndex() < blockIndex)
            {
                return it->get()->targetVersion();
            }
        }

        return BLOCK_MAJOR_VERSION_1;
    }

} // namespace CryptoNote
