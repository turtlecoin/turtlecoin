// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#undef NDEBUG

#include <iostream>
#include <chrono>
#include <assert.h>

#include "CryptoNote.h"
#include "CryptoTypes.h"
#include "Common/StringTools.h"
#include "crypto/crypto.h"

#define PERFORMANCE_ITERATIONS  1000

using namespace Crypto;
using namespace CryptoNote;

const std::string INPUT_DATA = "0100fb8e8ac805899323371bb790db19218afd8db8e3755d8b90f39b3d5506a9abce4fa912244500000000ee8146d49fa93ee724deb57d12cbc6c6f3b924d946127c7a97418f9348828f0f02";

const std::string CN_FAST_HASH = "b542df5b6e7f5f05275c98e7345884e2ac726aeeb07e03e44e0389eb86cd05f0";

const std::string CN_SLOW_HASH_V0 = "1b606a3f4a07d6489a1bcd07697bd16696b61c8ae982f61a90160f4e52828a7f";
const std::string CN_SLOW_HASH_V1 = "c9fae8425d8688dc236bcdbc42fdb42d376c6ec190501aa84b04a4b4cf1ee122";
const std::string CN_SLOW_HASH_V2 = "871fcd6823f6a879bb3f33951c8e8e891d4043880b02dfa1bb3be498b50e7578";

const std::string CN_LITE_SLOW_HASH_V0 = "28a22bad3f93d1408fca472eb5ad1cbe75f21d053c8ce5b3af105a57713e21dd";
const std::string CN_LITE_SLOW_HASH_V1 = "87c4e570653eb4c2b42b7a0d546559452dfab573b82ec52f152b7ff98e79446f";
const std::string CN_LITE_SLOW_HASH_V2 = "b7e78fab22eb19cb8c9c3afe034fb53390321511bab6ab4915cd538a630c3c62";

const std::string CN_SOFT_SHELL_V0[] = {
  "5e1891a15d5d85c09baf4a3bbe33675cfa3f77229c8ad66c01779e590528d6d3",
  "e1239347694df77cab780b7ec8920ec6f7e48ecef1d8c368e06708c08e1455f1",
  "118a03801c564d12f7e68972419303fe06f7a54ab8f44a8ce7deafbc6b1b5183",
  "8be48f7955eb3f9ac2275e445fe553f3ef359ea5c065cde98ff83011f407a0ec",
  "d33da3541960046e846530dcc9872b1914a62c09c7d732bff03bec481866ae48",
  "8be48f7955eb3f9ac2275e445fe553f3ef359ea5c065cde98ff83011f407a0ec",
  "118a03801c564d12f7e68972419303fe06f7a54ab8f44a8ce7deafbc6b1b5183",
  "e1239347694df77cab780b7ec8920ec6f7e48ecef1d8c368e06708c08e1455f1",
  "5e1891a15d5d85c09baf4a3bbe33675cfa3f77229c8ad66c01779e590528d6d3",
  "e1239347694df77cab780b7ec8920ec6f7e48ecef1d8c368e06708c08e1455f1",
  "118a03801c564d12f7e68972419303fe06f7a54ab8f44a8ce7deafbc6b1b5183",
  "8be48f7955eb3f9ac2275e445fe553f3ef359ea5c065cde98ff83011f407a0ec",
  "d33da3541960046e846530dcc9872b1914a62c09c7d732bff03bec481866ae48",
  "8be48f7955eb3f9ac2275e445fe553f3ef359ea5c065cde98ff83011f407a0ec",
  "118a03801c564d12f7e68972419303fe06f7a54ab8f44a8ce7deafbc6b1b5183",
  "e1239347694df77cab780b7ec8920ec6f7e48ecef1d8c368e06708c08e1455f1",
  "5e1891a15d5d85c09baf4a3bbe33675cfa3f77229c8ad66c01779e590528d6d3"
};

const std::string CN_SOFT_SHELL_V1[] = {
  "ae7f864a7a2f2b07dcef253581e60a014972b9655a152341cb989164761c180a",
  "ce8687bdd08c49bd1da3a6a74bf28858670232c1a0173ceb2466655250f9c56d",
  "ddb6011d400ac8725995fb800af11646bb2fef0d8b6136b634368ad28272d7f4",
  "02576f9873dc9c8b1b0fc14962982734dfdd41630fc936137a3562b8841237e1",
  "d37e2785ab7b3d0a222940bf675248e7b96054de5c82c5f0b141014e136eadbc",
  "02576f9873dc9c8b1b0fc14962982734dfdd41630fc936137a3562b8841237e1",
  "ddb6011d400ac8725995fb800af11646bb2fef0d8b6136b634368ad28272d7f4",
  "ce8687bdd08c49bd1da3a6a74bf28858670232c1a0173ceb2466655250f9c56d",
  "ae7f864a7a2f2b07dcef253581e60a014972b9655a152341cb989164761c180a",
  "ce8687bdd08c49bd1da3a6a74bf28858670232c1a0173ceb2466655250f9c56d",
  "ddb6011d400ac8725995fb800af11646bb2fef0d8b6136b634368ad28272d7f4",
  "02576f9873dc9c8b1b0fc14962982734dfdd41630fc936137a3562b8841237e1",
  "d37e2785ab7b3d0a222940bf675248e7b96054de5c82c5f0b141014e136eadbc",
  "02576f9873dc9c8b1b0fc14962982734dfdd41630fc936137a3562b8841237e1",
  "ddb6011d400ac8725995fb800af11646bb2fef0d8b6136b634368ad28272d7f4",
  "ce8687bdd08c49bd1da3a6a74bf28858670232c1a0173ceb2466655250f9c56d",
  "ae7f864a7a2f2b07dcef253581e60a014972b9655a152341cb989164761c180a"
};

const std::string CN_SOFT_SHELL_V2[] = {
  "b2172ec9466e1aee70ec8572a14c233ee354582bcb93f869d429744de5726a26",
  "b2623a2b041dc5ae3132b964b75e193558c7095e725d882a3946aae172179cf1",
  "141878a7b58b0f57d00b8fc2183cce3517d9d68becab6fee52abb3c1c7d0805b",
  "4646f9919791c28f0915bc0005ed619bee31d42359f7a8af5de5e1807e875364",
  "3fedc7ab0f8d14122fc26062de1af7a6165755fcecdf0f12fa3ccb3ff63629d0",
  "4646f9919791c28f0915bc0005ed619bee31d42359f7a8af5de5e1807e875364",
  "141878a7b58b0f57d00b8fc2183cce3517d9d68becab6fee52abb3c1c7d0805b",
  "b2623a2b041dc5ae3132b964b75e193558c7095e725d882a3946aae172179cf1",
  "b2172ec9466e1aee70ec8572a14c233ee354582bcb93f869d429744de5726a26",
  "b2623a2b041dc5ae3132b964b75e193558c7095e725d882a3946aae172179cf1",
  "141878a7b58b0f57d00b8fc2183cce3517d9d68becab6fee52abb3c1c7d0805b",
  "4646f9919791c28f0915bc0005ed619bee31d42359f7a8af5de5e1807e875364",
  "3fedc7ab0f8d14122fc26062de1af7a6165755fcecdf0f12fa3ccb3ff63629d0",
  "4646f9919791c28f0915bc0005ed619bee31d42359f7a8af5de5e1807e875364",
  "141878a7b58b0f57d00b8fc2183cce3517d9d68becab6fee52abb3c1c7d0805b",
  "b2623a2b041dc5ae3132b964b75e193558c7095e725d882a3946aae172179cf1",
  "b2172ec9466e1aee70ec8572a14c233ee354582bcb93f869d429744de5726a26"
};

static inline bool CompareHashes(const Hash leftHash, const std::string right)
{
  Hash rightHash = Hash();
  if (!Common::podFromHex(right, rightHash)) {
    return false;
  }

  return (leftHash == rightHash);
}

int main(int argc, char** argv) {
  try {
    const BinaryArray& rawData = Common::fromHex(INPUT_DATA);

    std::cout << "\nInput: " << INPUT_DATA << "\n\n";

    Hash hash = Hash();

    cn_fast_hash(rawData.data(), rawData.size(), hash);
    std::cout << "cn_fast_hash: " << Common::toHex(&hash, sizeof(Hash)) << "\n";
    assert(CompareHashes(hash, CN_FAST_HASH));

    std::cout << "\n";

    cn_slow_hash_v0(rawData.data(), rawData.size(), hash);
    std::cout << "cn_slow_hash_v0: " << Common::toHex(&hash, sizeof(Hash)) << "\n";
    assert(CompareHashes(hash, CN_SLOW_HASH_V0));

    if (rawData.size() >= 43)
    {
      cn_slow_hash_v1(rawData.data(), rawData.size(), hash);
      std::cout << "cn_slow_hash_v1: " << Common::toHex(&hash, sizeof(Hash)) << "\n";
      assert(CompareHashes(hash, CN_SLOW_HASH_V1));

      cn_slow_hash_v2(rawData.data(), rawData.size(), hash);
      std::cout << "cn_slow_hash_v2: " << Common::toHex(&hash, sizeof(Hash)) << "\n";
      assert(CompareHashes(hash, CN_SLOW_HASH_V2));

      std::cout << "\n";

      cn_lite_slow_hash_v0(rawData.data(), rawData.size(), hash);
      std::cout << "cn_lite_slow_hash_v0: " << Common::toHex(&hash, sizeof(Hash)) << "\n";
      assert(CompareHashes(hash, CN_LITE_SLOW_HASH_V0));

      cn_lite_slow_hash_v1(rawData.data(), rawData.size(), hash);
      std::cout << "cn_lite_slow_hash_v1: " << Common::toHex(&hash, sizeof(Hash)) << "\n";
      assert(CompareHashes(hash, CN_LITE_SLOW_HASH_V1));

      cn_lite_slow_hash_v2(rawData.data(), rawData.size(), hash);
      std::cout << "cn_lite_slow_hash_v2: " << Common::toHex(&hash, sizeof(Hash)) << "\n";
      assert(CompareHashes(hash, CN_LITE_SLOW_HASH_V2));

      std::cout << "\n";

      for (uint32_t height = 0; height <= 8192; height = height + 512)
      {
        cn_soft_shell_slow_hash_v0(rawData.data(), rawData.size(), hash, height);
        std::cout << "cn_soft_shell_slow_hash_v0 (" << height << "): " << Common::toHex(&hash, sizeof(Hash)) << "\n";
        assert(CompareHashes(hash, CN_SOFT_SHELL_V0[height / 512]));
      }

      std::cout << "\n";

      for (uint32_t height = 0; height <= 8192; height = height + 512)
      {
        cn_soft_shell_slow_hash_v1(rawData.data(), rawData.size(), hash, height);
        std::cout << "cn_soft_shell_slow_hash_v1 (" << height << "): " << Common::toHex(&hash, sizeof(Hash)) << "\n";
        assert(CompareHashes(hash, CN_SOFT_SHELL_V1[height / 512]));
      }

      std::cout << "\n";

      for (uint32_t height = 0; height <= 8192; height = height + 512)
      {
        cn_soft_shell_slow_hash_v2(rawData.data(), rawData.size(), hash, height);
        std::cout << "cn_soft_shell_slow_hash_v2 (" << height << "): " << Common::toHex(&hash, sizeof(Hash)) << "\n";
        assert(CompareHashes(hash, CN_SOFT_SHELL_V2[height / 512]));
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