// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "Difficulty.h"

#include <algorithm>
#include <config/CryptoNoteConfig.h>

// LWMA-2 difficulty algorithm
// Copyright (c) 2017-2018 Zawy, MIT License
// https://github.com/zawy12/difficulty-algorithms/issues/3
uint64_t nextDifficultyV5(std::vector<uint64_t> timestamps, std::vector<uint64_t> cumulativeDifficulties)
{
    int64_t T = CryptoNote::parameters::DIFFICULTY_TARGET;
    int64_t N = CryptoNote::parameters::DIFFICULTY_WINDOW_V3;
    int64_t L(0), ST, sum_3_ST(0), next_D, prev_D;

    /* If we are starting up, returning a difficulty guess. If you are a
       new coin, you might want to set this to a decent estimate of your
       hashrate */
    if (timestamps.size() < static_cast<uint64_t>(N + 1))
    {
        return 10000;
    }

    for (int64_t i = 1; i <= N; i++)
    {
        ST = static_cast<int64_t>(timestamps[i]) - static_cast<int64_t>(timestamps[i - 1]);

        ST = std::max(-4 * T, std::min(ST, 6 * T));

        L += ST * i;

        if (i > N - 3)
        {
            sum_3_ST += ST;
        }
    }

    next_D = (static_cast<int64_t>(cumulativeDifficulties[N] - cumulativeDifficulties[0]) * T * (N + 1) * 99)
             / (100 * 2 * L);

    prev_D = cumulativeDifficulties[N] - cumulativeDifficulties[N - 1];

    next_D = std::max((prev_D * 67) / 100, std::min(next_D, (prev_D * 150) / 100));

    if (sum_3_ST < (8 * T) / 10)
    {
        next_D = std::max(next_D, (prev_D * 108) / 100);
    }

    return static_cast<uint64_t>(next_D);
}

// LWMA-2 difficulty algorithm
// Copyright (c) 2017-2018 Zawy, MIT License
// https://github.com/zawy12/difficulty-algorithms/issues/3
uint64_t nextDifficultyV4(std::vector<uint64_t> timestamps, std::vector<uint64_t> cumulativeDifficulties)
{
    int64_t T = CryptoNote::parameters::DIFFICULTY_TARGET;
    int64_t N = CryptoNote::parameters::DIFFICULTY_WINDOW_V3;
    int64_t L(0), ST, sum_3_ST(0), next_D, prev_D;

    if (timestamps.size() <= static_cast<uint64_t>(N))
    {
        return 1000;
    }

    for (int64_t i = 1; i <= N; i++)
    {
        ST = clamp(-6 * T, static_cast<int64_t>(timestamps[i]) - static_cast<int64_t>(timestamps[i - 1]), 6 * T);

        L += ST * i;

        if (i > N - 3)
        {
            sum_3_ST += ST;
        }
    }

    next_D = (static_cast<int64_t>(cumulativeDifficulties[N] - cumulativeDifficulties[0]) * T * (N + 1) * 99)
             / (100 * 2 * L);
    prev_D = cumulativeDifficulties[N] - cumulativeDifficulties[N - 1];

    /* Make sure we don't divide by zero if 50x attacker (thanks fireice) */
    next_D = std::max((prev_D * 67) / 100, std::min(next_D, (prev_D * 150) / 100));

    if (sum_3_ST < (8 * T) / 10)
    {
        next_D = std::max(next_D, (prev_D * 110) / 100);
    }

    return static_cast<uint64_t>(next_D);
}

// LWMA-2 difficulty algorithm
// Copyright (c) 2017-2018 Zawy, MIT License
// https://github.com/zawy12/difficulty-algorithms/issues/3
uint64_t nextDifficultyV3(std::vector<uint64_t> timestamps, std::vector<uint64_t> cumulativeDifficulties)
{
    int64_t T = CryptoNote::parameters::DIFFICULTY_TARGET;
    int64_t N = CryptoNote::parameters::DIFFICULTY_WINDOW_V3;
    int64_t FTL = CryptoNote::parameters::CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V3;
    int64_t L(0), ST, sum_3_ST(0), next_D, prev_D;

    if (timestamps.size() <= static_cast<uint64_t>(N))
    {
        return 1000;
    }

    for (int64_t i = 1; i <= N; i++)
    {
        ST = std::max(
            -FTL, std::min(static_cast<int64_t>(timestamps[i]) - static_cast<int64_t>(timestamps[i - 1]), 6 * T));

        L += ST * i;

        if (i > N - 3)
        {
            sum_3_ST += ST;
        }
    }

    next_D = (static_cast<int64_t>(cumulativeDifficulties[N] - cumulativeDifficulties[0]) * T * (N + 1) * 99)
             / (100 * 2 * L);
    prev_D = cumulativeDifficulties[N] - cumulativeDifficulties[N - 1];

    /* Make sure we don't divide by zero if 50x attacker (thanks fireice) */
    next_D = std::max((prev_D * 70) / 100, std::min(next_D, (prev_D * 107) / 100));

    if (sum_3_ST < (8 * T) / 10)
    {
        next_D = (prev_D * 110) / 100;
    }

    return static_cast<uint64_t>(next_D);
}

// Difficulty reset helper
uint64_t adjustForDifficultyReset(const uint64_t nextDifficulty, const uint64_t blockIndex)
{
    /* This code supports the difficulty reset as activated by the specified height as required.
       We allow the normal difficulty calculation to run unimpeded and then multiply the calculated
       difficulty by the multiplier to arrive at our adjusted difficulty for the reset window specified */
    if (blockIndex >= CryptoNote::parameters::DIFFICULTY_RESET_HEIGHT_V1
        && blockIndex < CryptoNote::parameters::DIFFICULTY_RESET_HEIGHT_V1
                            + CryptoNote::parameters::DIFFICULTY_RESET_WINDOW_V1)
    {
        /* Calculate our adjusted difficulty multiplier */
        float currentDifficultyMultiplier = calculateDifficultyResetMultiplier(
            blockIndex,
            CryptoNote::parameters::DIFFICULTY_RESET_HEIGHT_V1,
            CryptoNote::parameters::DIFFICULTY_RESET_WINDOW_V1,
            CryptoNote::parameters::DIFFICULTY_RESET_MULTIPLIER_V1);

        /* We use that updated difficulty multiplier to calculate our adjusted difficulty */
        return static_cast<uint64_t>(nextDifficulty * currentDifficultyMultiplier);
    }

    return nextDifficulty;
}

/* Helper method that helps us to calculate a difficulty reset with a ramp
   towards returning full control to the normal DA */
float calculateDifficultyResetMultiplier(
    const uint64_t blockIndex,
    const uint64_t resetHeight,
    const uint64_t resetWindow,
    const float resetMultiplier)
{
    /* First, we calculate how far into the reset window we are */
    uint64_t currentDifficultyResetDepth = blockIndex - resetHeight;
    float currentDifficultyResetDepthPercent = currentDifficultyResetDepth / resetWindow;

    /* Then we figure out the inverse of the multiplier specified (the difficulty reduction)
      that is being applied */
    float currentDifficultyReduction = 1 - resetMultiplier;

    /* Now we can figure out what our adjusted multiplier is by multiplying our reset depth percentage
      by the overall difficulty reduction and add the result to the base multiplier */
    float currentDifficultyMultiplier =
        (currentDifficultyResetDepthPercent * currentDifficultyReduction) + resetMultiplier;

    return currentDifficultyMultiplier;
}
