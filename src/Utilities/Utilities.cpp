// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

////////////////////////////////
#include <Utilities/Utilities.h>
////////////////////////////////

#include <atomic>

#include <Common/Base58.h>

#include <config/CryptoNoteConfig.h>

#include <CryptoNoteCore/CryptoNoteBasicImpl.h>
#include <CryptoNoteCore/CryptoNoteTools.h>

#include <thread>

namespace Utilities
{

uint64_t getTransactionSum(const std::vector<std::pair<std::string, uint64_t>> destinations)
{
    uint64_t amountSum = 0;

    for (const auto &[destination, amount] : destinations)
    {
        amountSum += amount;
    }

    return amountSum;
}

/* Round value to the nearest multiple (rounding down) */
uint64_t getLowerBound(const uint64_t val, const uint64_t nearestMultiple)
{
    uint64_t remainder = val % nearestMultiple;

    return val - remainder;
}

/* Round value to the nearest multiple (rounding down) */
uint64_t getUpperBound(const uint64_t val, const uint64_t nearestMultiple)
{
    return getLowerBound(val, nearestMultiple) + nearestMultiple;
}

bool isInputUnlocked(const uint64_t unlockTime, const uint64_t currentHeight)
{
    /* Might as well return fast with the case that is true for nearly all
       transactions (excluding coinbase) */
    if (unlockTime == 0)
    {
        return true;
    }

    /* if unlockTime is greater than this amount, we treat it as a
       timestamp, otherwise we treat it as a block height */
    if (unlockTime >= CryptoNote::parameters::CRYPTONOTE_MAX_BLOCK_NUMBER)
    {
        const uint64_t currentTimeAdjusted = static_cast<uint64_t>(std::time(nullptr)) +
                                             CryptoNote::parameters::CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS;

        return currentTimeAdjusted >= unlockTime;
    }

    const uint64_t currentHeightAdjusted =
        currentHeight + CryptoNote::parameters::CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS;

    return currentHeightAdjusted >= unlockTime;
}

/* The formula for the block size is as follows. Calculate the
   maxBlockCumulativeSize. This is equal to:
   100,000 + ((height * 102,400) / 1,051,200)
   At a block height of 400k, this gives us a size of 138,964.
   The constants this calculation arise from can be seen below, or in
   src/CryptoNoteCore/Currency.cpp::maxBlockCumulativeSize(). Call this value
   x.

   Next, calculate the median size of the last 100 blocks. Take the max of
   this value, and 100,000. Multiply this value by 1.25. Call this value y.

   Finally, return the minimum of x and y.

   Or, in short: min(140k (slowly rising), 1.25 * max(100k, median(last 100 blocks size)))
   Block size will always be 125k or greater (Assuming non testnet)

   To get the max transaction size, remove 600 from this value, for the
   reserved miner transaction.

   We are going to ignore the median(last 100 blocks size), as it is possible
   for a transaction to be valid for inclusion in a block when it is submitted,
   but not when it actually comes to be mined, for example if the median
   block size suddenly decreases. This gives a bit of a lower cap of max
   tx sizes, but prevents anything getting stuck in the pool.

*/
uint64_t getMaxTxSize(const uint64_t currentHeight)
{
    const uint64_t numerator = currentHeight * CryptoNote::parameters::MAX_BLOCK_SIZE_GROWTH_SPEED_NUMERATOR;
    const uint64_t denominator = CryptoNote::parameters::MAX_BLOCK_SIZE_GROWTH_SPEED_DENOMINATOR;

    const uint64_t growth = numerator / denominator;

    const uint64_t x = CryptoNote::parameters::MAX_BLOCK_SIZE_INITIAL + growth;

    const uint64_t y = 125000;

    /* Need space for the miner transaction */
    return std::min(x, y) - CryptoNote::parameters::CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE;
}

/* Sleep for approximately duration, unless condition is true. This lets us
   not bother the node too often, but makes shutdown times still quick. */
void sleepUnlessStopping(const std::chrono::milliseconds duration, std::atomic<bool> &condition)
{
    auto sleptFor = std::chrono::milliseconds::zero();

    /* 0.5 seconds */
    const auto sleepDuration = std::chrono::milliseconds(500);

    while (!condition && sleptFor < duration)
    {
        std::this_thread::sleep_for(sleepDuration);

        sleptFor += sleepDuration;
    }
}

/* Converts a height to a timestamp */
uint64_t scanHeightToTimestamp(const uint64_t scanHeight)
{
    if (scanHeight == 0)
    {
        return 0;
    }

    /* Get the amount of seconds since the blockchain launched */
    uint64_t secondsSinceLaunch = scanHeight * CryptoNote::parameters::DIFFICULTY_TARGET;

    /* Get the genesis block timestamp and add the time since launch */
    uint64_t timestamp = CryptoNote::parameters::GENESIS_BLOCK_TIMESTAMP + secondsSinceLaunch;

    /* Don't make timestamp too large or daemon throws an error */
    if (timestamp >= getCurrentTimestampAdjusted())
    {
        return getCurrentTimestampAdjusted();
    }

    return timestamp;
}

uint64_t timestampToScanHeight(const uint64_t timestamp)
{
    if (timestamp == 0)
    {
        return 0;
    }

    /* Timestamp is before the chain launched! */
    if (timestamp <= CryptoNote::parameters::GENESIS_BLOCK_TIMESTAMP)
    {
        return 0;
    }

    /* Find the amount of seconds between launch and the timestamp */
    uint64_t launchTimestampDelta = timestamp - CryptoNote::parameters::GENESIS_BLOCK_TIMESTAMP;

    /* Get an estimation of the amount of blocks that have passed before the
       timestamp */
    return launchTimestampDelta / CryptoNote::parameters::DIFFICULTY_TARGET;
}

uint64_t getCurrentTimestampAdjusted()
{
    /* Get the current time as a unix timestamp */
    std::time_t time = std::time(nullptr);

    /* Take the amount of time a block can potentially be in the past/future */
    std::initializer_list<uint64_t> limits = {CryptoNote::parameters::CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT,
                                              CryptoNote::parameters::CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V3,
                                              CryptoNote::parameters::CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V4};

    /* Get the largest adjustment possible */
    uint64_t adjust = std::max(limits);

    /* Take the earliest timestamp that will include all possible blocks */
    return time - adjust;
}

} // namespace Utilities
