// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <iostream>

#include "CryptoNote.h"
#include "CryptoTypes.h"
#include "Common/StringTools.h"
#include "crypto/crypto.h"

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
    std::cout << "Cryptonight Fast Hash: " << Common::toHex(&hash, sizeof(Hash)) << "\n";

    cn_slow_hash_v0(rawData.data(), rawData.size(), hash);
    std::cout << "Cryptonight v0: " << Common::toHex(&hash, sizeof(Hash)) << "\n";

    if (rawData.size() >= 43)
    {
      cn_slow_hash_v1(rawData.data(), rawData.size(), hash);
      std::cout << "Cryptonight v1: " << Common::toHex(&hash, sizeof(Hash)) << "\n";

      cn_lite_slow_hash_v0(rawData.data(), rawData.size(), hash);
      std::cout << "Cryptonight Lite v0: " << Common::toHex(&hash, sizeof(Hash)) << "\n";

      cn_lite_slow_hash_v1(rawData.data(), rawData.size(), hash);
      std::cout << "Cryptonight Lite v1: " << Common::toHex(&hash, sizeof(Hash)) << "\n";
    }
  }
  catch (std::exception& e)
  {
    std::cout << "Something went terribly wrong...\n" << e.what() << "\n\n";
  }

  return 0;
}