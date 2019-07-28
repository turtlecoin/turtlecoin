// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <stdint.h>
#include <vector>

uint64_t nextDifficultyV5(std::vector<uint64_t> timestamps, std::vector<uint64_t> cumulativeDifficulties);

uint64_t nextDifficultyV4(std::vector<uint64_t> timestamps, std::vector<uint64_t> cumulativeDifficulties);

uint64_t nextDifficultyV3(std::vector<uint64_t> timestamps, std::vector<uint64_t> cumulativeDifficulties);

template<typename T> T clamp(const T &n, const T &lower, const T &upper)
{
    return std::max(lower, std::min(n, upper));
}
