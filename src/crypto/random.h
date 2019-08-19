// Copyright (c) 2019, The TurtleCoin Developers

#pragma once

#include <random>

namespace Random
{
    /* Used to obtain a random seed */
    static thread_local std::random_device device;

    /* Generator, seeded with the random device */
    static thread_local std::mt19937 gen(device());

    /* The distribution to get numbers for - in this case, uint8_t */
    static std::uniform_int_distribution<int> distribution {0, std::numeric_limits<uint8_t>::max()};

    /**
     * Generate n random bytes (uint8_t), and place them in *result. Result should be large
     * enough to contain the bytes.
     */
    inline void randomBytes(size_t n, uint8_t *result)
    {
        for (size_t i = 0; i < n; i++)
        {
            result[i] = distribution(gen);
        }
    }

    /**
     * Generate n random bytes (uint8_t), and return them in a vector.
     */
    inline std::vector<uint8_t> randomBytes(size_t n)
    {
        std::vector<uint8_t> result;

        result.reserve(n);

        for (size_t i = 0; i < n; i++)
        {
            result.push_back(distribution(gen));
        }

        return result;
    }

    /**
     * Generate a random value of the type specified, in the full range of the
     * type
     */
    template<typename T> T randomValue()
    {
        std::uniform_int_distribution<T> distribution {std::numeric_limits<T>::min(), std::numeric_limits<T>::max()};

        return distribution(gen);
    }

    /**
     * Generate a random value of the type specified, in the range [min, max]
     * Note that both min, and max, are included in the results. Therefore,
     * randomValue(0, 100), will generate numbers between 0 and 100.
     *
     * Note that min must be <= max, or undefined behaviour will occur.
     */
    template<typename T> T randomValue(T min, T max)
    {
        std::uniform_int_distribution<T> distribution {min, max};
        return distribution(gen);
    }

    /**
     * Obtain the generator used internally. Helpful for passing to functions
     * like std::shuffle.
     */
    inline auto generator()
    {
        return gen;
    }
} // namespace Random
