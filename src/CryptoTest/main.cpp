// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <iostream>
#include <chrono>

#include "CryptoNote.h"
#include "CryptoTypes.h"
#include "Common/StringTools.h"
#include "crypto/crypto.h"

#define PERFORMANCE_ITERATIONS  1000

using namespace Crypto;
using namespace CryptoNote;

int main(int argc, char** argv) {
  try {
    if (argc != 2)
    {
      throw std::runtime_error("Must supply data to hash");
    }

    std::string input = argv[1];

    const BinaryArray& rawData = Common::fromHex(input);

    std::cout << "\nInput: " << input << "\n\n";

    Hash hash = Hash();

    cn_fast_hash(rawData.data(), rawData.size(), hash);
    std::cout << "cn_fast_hash: " << Common::toHex(&hash, sizeof(Hash)) << "\n";

    cn_slow_hash_v0(rawData.data(), rawData.size(), hash);
    std::cout << "cn_slow_hash_v0: " << Common::toHex(&hash, sizeof(Hash)) << "\n";

    if (rawData.size() >= 43)
    {
      cn_slow_hash_v1(rawData.data(), rawData.size(), hash);
      std::cout << "cn_slow_hash_v1: " << Common::toHex(&hash, sizeof(Hash)) << "\n";

      cn_slow_hash_v2(rawData.data(), rawData.size(), hash);
      std::cout << "cn_slow_hash_v2: " << Common::toHex(&hash, sizeof(Hash)) << "\n";

      cn_lite_slow_hash_v0(rawData.data(), rawData.size(), hash);
      std::cout << "cn_lite_slow_hash_v0: " << Common::toHex(&hash, sizeof(Hash)) << "\n";

      cn_lite_slow_hash_v1(rawData.data(), rawData.size(), hash);
      std::cout << "cn_lite_slow_hash_v1: " << Common::toHex(&hash, sizeof(Hash)) << "\n";

      cn_lite_slow_hash_v2(rawData.data(), rawData.size(), hash);
      std::cout << "cn_lite_slow_hash_v2: " << Common::toHex(&hash, sizeof(Hash)) << "\n";

      std::cout << "\n";

      for (uint32_t height = 0; height <= 8192; height = height + 512)
      {
        cn_soft_shell_slow_hash_v0(rawData.data(), rawData.size(), hash, height);
        std::cout << "cn_soft_shell_slow_hash_v0 (" << height << "): " << Common::toHex(&hash, sizeof(Hash)) << "\n";
      }

      std::cout << "\n";

      for (uint32_t height = 0; height <= 8192; height = height + 512)
      {
        cn_soft_shell_slow_hash_v1(rawData.data(), rawData.size(), hash, height);
        std::cout << "cn_soft_shell_slow_hash_v1 (" << height << "): " << Common::toHex(&hash, sizeof(Hash)) << "\n";
      }

      std::cout << "\n";

      for (uint32_t height = 0; height <= 8192; height = height + 512)
      {
        cn_soft_shell_slow_hash_v2(rawData.data(), rawData.size(), hash, height);
        std::cout << "cn_soft_shell_slow_hash_v2 (" << height << "): " << Common::toHex(&hash, sizeof(Hash)) << "\n";
      }
    }

    std::cout <<  "\nPerformance Tests: Please wait, this may take a while depending on your system...\n\n";

    auto startTimer = std::chrono::high_resolution_clock::now();
    for (auto i = 0; i < PERFORMANCE_ITERATIONS; i++)
    {
      cn_slow_hash_v0(rawData.data(), rawData.size(), hash);
    }
    auto elapsedTime = std::chrono::high_resolution_clock::now() - startTimer;
    std::cout << "cn_slow_hash_v0: " << (PERFORMANCE_ITERATIONS / std::chrono::duration_cast<std::chrono::seconds>(elapsedTime).count()) << " H/s\n";

    startTimer = std::chrono::high_resolution_clock::now();
    for (auto i = 0; i < PERFORMANCE_ITERATIONS; i++)
    {
      cn_lite_slow_hash_v0(rawData.data(), rawData.size(), hash);
    }
    elapsedTime = std::chrono::high_resolution_clock::now() - startTimer;
    std::cout << "cn_lite_slow_hash_v0: " << (PERFORMANCE_ITERATIONS / std::chrono::duration_cast<std::chrono::seconds>(elapsedTime).count()) << " H/s\n";
  }
  catch (std::exception& e)
  {
    std::cout << "Something went terribly wrong...\n" << e.what() << "\n\n";
  }

  return 0;
}