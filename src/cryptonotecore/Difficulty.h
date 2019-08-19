// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <stdint.h>
#include <vector>

uint64_t nextDifficultyV5(std::vector<uint64_t> timestamps, std::vector<uint64_t> cumulativeDifficulties);

uint64_t nextDifficultyV4(std::vector<uint64_t> timestamps, std::vector<uint64_t> cumulativeDifficulties);

uint64_t nextDifficultyV3(std::vector<uint64_t> timestamps, std::vector<uint64_t> cumulativeDifficulties);

uint64_t adjustForDifficultyReset(const uint64_t nextDifficulty, const uint64_t blockIndex);

float calculateDifficultyResetMultiplier(
    const uint64_t blockIndex,
    const uint64_t resetHeight,
    const uint64_t resetWindow,
    const float resetMultiplier);

template<typename T> T clamp(const T &n, const T &lower, const T &upper)
{
    return std::max(lower, std::min(n, upper));
}
