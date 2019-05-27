// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <iostream>
#include <chrono>
#include <assert.h>

#include <cxxopts.hpp>
#include <config/CliHeader.h>

#include "CryptoNote.h"
#include "CryptoTypes.h"
#include "Common/StringTools.h"
#include "crypto/crypto.h"

#define PERFORMANCE_ITERATIONS  1000
#define PERFORMANCE_ITERATIONS_LONG_MULTIPLIER 10

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

const std::string CN_DARK_SLOW_HASH_V0 = "bea42eadd78614f875e55bb972aa5ec54a5edf2dd7068220fda26bf4b1080fb8";
const std::string CN_DARK_SLOW_HASH_V1 = "d18cb32bd5b465e5a7ba4763d60f88b5792f24e513306f1052954294b737e871";
const std::string CN_DARK_SLOW_HASH_V2 = "a18a14d94efea108757a42633a1b4d4dc11838084c3c4347850d39ab5211a91f";

const std::string CN_DARK_LITE_SLOW_HASH_V0 = "faa7884d9c08126eb164814aeba6547b5d6064277a09fb6b414f5dbc9d01eb2b";
const std::string CN_DARK_LITE_SLOW_HASH_V1 = "c75c010780fffd9d5e99838eb093b37c0dd015101c9d298217866daa2993d277";
const std::string CN_DARK_LITE_SLOW_HASH_V2 = "fdceb794c1055977a955f31c576a8be528a0356ee1b0a1f9b7f09e20185cda28";

const std::string CN_TURTLE_SLOW_HASH_V0 = "546c3f1badd7c1232c7a3b88cdb013f7f611b7bd3d1d2463540fccbd12997982";
const std::string CN_TURTLE_SLOW_HASH_V1 = "29e7831780a0ab930e0fe3b965f30e8a44d9b3f9ad2241d67cfbfea3ed62a64e";
const std::string CN_TURTLE_SLOW_HASH_V2 = "fc67dfccb5fc90d7855ae903361eabd76f1e40a22a72ad3ef2d6ad27b5a60ce5";

const std::string CN_TURTLE_LITE_SLOW_HASH_V0 = "5e1891a15d5d85c09baf4a3bbe33675cfa3f77229c8ad66c01779e590528d6d3";
const std::string CN_TURTLE_LITE_SLOW_HASH_V1 = "ae7f864a7a2f2b07dcef253581e60a014972b9655a152341cb989164761c180a";
const std::string CN_TURTLE_LITE_SLOW_HASH_V2 = "b2172ec9466e1aee70ec8572a14c233ee354582bcb93f869d429744de5726a26";


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

/* Check if we're testing a v1 or v2 hash function */
/* Hacky as fuck lmao */
bool need43BytesOfData(std::string hashFunctionName)
{
    return (hashFunctionName.find("v1") != std::string::npos 
        || hashFunctionName.find("v2") != std::string::npos);
}

/* Bit of hackery so we can get the variable name of the passed in function.
   This way we can print the test we are currently performing. */
#define TEST_HASH_FUNCTION(hashFunction, expectedOutput) \
   testHashFunction(hashFunction, expectedOutput, #hashFunction, -1)

#define TEST_HASH_FUNCTION_WITH_HEIGHT(hashFunction, expectedOutput, height) \
    testHashFunction(hashFunction, expectedOutput, #hashFunction, height, height)

template<typename T, typename ...Args>
void testHashFunction(
    T hashFunction,
    std::string expectedOutput,
    std::string hashFunctionName,
    int64_t height,
    Args && ... args)
{
    const BinaryArray& rawData = Common::fromHex(INPUT_DATA);

    if (need43BytesOfData(hashFunctionName) && rawData.size() < 43)
    {
        return;
    }

    Hash hash = Hash();

    /* Perform the hash, with a height if given */
    hashFunction(rawData.data(), rawData.size(), hash, std::forward<Args>(args)...);

    if (height == -1)
    {
        std::cout << hashFunctionName << ": " << hash << std::endl;
    }
    else
    {
        std::cout << hashFunctionName << " (" << height << "): " << hash << std::endl;
    }

    /* Verify the hash is as expected */
    if(!CompareHashes(hash, expectedOutput))
    {
        std::cout << "Hashes are not equal!\n" << "Expected: " << expectedOutput << "\nActual: " << hash
                  << "\nTerminating.";

        exit(1);
    }
}

/* Bit of hackery so we can get the variable name of the passed in function.
   This way we can print the test we are currently performing. */
#define BENCHMARK(hashFunction, iterations) \
   benchmark(hashFunction, #hashFunction, iterations)

template<typename T>
void benchmark(T hashFunction, std::string hashFunctionName, uint64_t iterations)
{
    const BinaryArray& rawData = Common::fromHex(INPUT_DATA);

    if (need43BytesOfData(hashFunctionName) && rawData.size() < 43)
    {
        return;
    }

    Hash hash = Hash();

    auto startTimer = std::chrono::high_resolution_clock::now();

    for (uint64_t i = 0; i < iterations; i++)
    {
        hashFunction(rawData.data(), rawData.size(), hash);
    }

    auto elapsedTime = std::chrono::high_resolution_clock::now() - startTimer;

    std::cout << hashFunctionName << ": "
              << (iterations / std::chrono::duration_cast<std::chrono::seconds>(elapsedTime).count())
              << " H/s\n";
}

void benchmarkUnderivePublicKey()
{
    Crypto::KeyDerivation derivation;

    Crypto::PublicKey txPublicKey;
    Common::podFromHex("f235acd76ee38ec4f7d95123436200f9ed74f9eb291b1454fbc30742481be1ab", txPublicKey);

    Crypto::SecretKey privateViewKey;
    Common::podFromHex("89df8c4d34af41a51cfae0267e8254cadd2298f9256439fa1cfa7e25ee606606", privateViewKey);

    Crypto::generate_key_derivation(txPublicKey, privateViewKey, derivation);

    const uint64_t loopIterations = 600000;

    auto startTimer = std::chrono::high_resolution_clock::now();

    Crypto::PublicKey spendKey;

    Crypto::PublicKey outputKey;
    Common::podFromHex("4a078e76cd41a3d3b534b83dc6f2ea2de500b653ca82273b7bfad8045d85a400", outputKey);

    for (uint64_t i = 0; i < loopIterations; i++)
    {
        /* Use i as output index to prevent optimization */
        Crypto::underive_public_key(derivation, i, outputKey, spendKey);
    }

    auto elapsedTime = std::chrono::high_resolution_clock::now() - startTimer;

    /* Need to use microseconds here then divide by 1000 - otherwise we'll just get '0' */
    const auto timePerDerivation = std::chrono::duration_cast<std::chrono::microseconds>(elapsedTime).count() / loopIterations;

    std::cout << "Time to perform underivePublicKey: " << timePerDerivation / 1000.0 << " ms" << std::endl;
}

void benchmarkGenerateKeyDerivation()
{
    Crypto::KeyDerivation derivation;

    Crypto::PublicKey txPublicKey;
    Common::podFromHex("f235acd76ee38ec4f7d95123436200f9ed74f9eb291b1454fbc30742481be1ab", txPublicKey);

    Crypto::SecretKey privateViewKey;
    Common::podFromHex("89df8c4d34af41a51cfae0267e8254cadd2298f9256439fa1cfa7e25ee606606", privateViewKey);

    const uint64_t loopIterations = 60000;

    auto startTimer = std::chrono::high_resolution_clock::now();

    for (uint64_t i = 0; i < loopIterations; i++)
    {
        Crypto::generate_key_derivation(txPublicKey, privateViewKey, derivation);
    }

    auto elapsedTime = std::chrono::high_resolution_clock::now() - startTimer;

    const auto timePerDerivation = std::chrono::duration_cast<std::chrono::microseconds>(elapsedTime).count() / loopIterations;

    std::cout << "Time to perform generateKeyDerivation: " << timePerDerivation / 1000.0 << " ms" << std::endl;
}

int main(int argc, char** argv)
{
    bool o_help, o_version, o_benchmark;
    int o_iterations;

    cxxopts::Options options(argv[0], getProjectCLIHeader());

    options.add_options("Core")
        ("h,help", "Display this help message", cxxopts::value<bool>(o_help)->implicit_value("true"))
        ("v,version", "Output software version information", cxxopts::value<bool>(o_version)->default_value("false")->implicit_value("true"));

    options.add_options("Performance Testing")
        ("b,benchmark", "Run quick performance benchmark", cxxopts::value<bool>(o_benchmark)->default_value("false")->implicit_value("true"))
        ("i,iterations", "The number of iterations for the benchmark test. Minimum of 1,000 iterations required.",
            cxxopts::value<int>(o_iterations)->default_value(std::to_string(PERFORMANCE_ITERATIONS)), "#");

    try
    {
        auto result = options.parse(argc, argv);
    }
    catch (const cxxopts::OptionException& e)
    {
        std::cout << "Error: Unable to parse command line argument options: " << e.what() << std::endl << std::endl;
        std::cout << options.help({}) << std::endl;
        exit(1);
    }

    if (o_help) // Do we want to display the help message?
    {
        std::cout << options.help({}) << std::endl;
        exit(0);
    }
    else if (o_version) // Do we want to display the software version?
    {
        std::cout << getProjectCLIHeader() << std::endl;
        exit(0);
    }

    if (o_iterations < 1000 && o_benchmark)
    {
        std::cout << std::endl << "Error: The number of --iterations should be at least 1,000 for reasonable accuracy" << std::endl;
        exit(1);
    }

    int o_iterations_long = o_iterations * PERFORMANCE_ITERATIONS_LONG_MULTIPLIER;

    try
    {
        std::cout << getProjectCLIHeader() << std::endl;

        std::cout << "Input: " << INPUT_DATA << std::endl << std::endl;

        TEST_HASH_FUNCTION(cn_slow_hash_v0, CN_SLOW_HASH_V0);
        TEST_HASH_FUNCTION(cn_slow_hash_v1, CN_SLOW_HASH_V1);
        TEST_HASH_FUNCTION(cn_slow_hash_v2, CN_SLOW_HASH_V2);
        
        std::cout << std::endl;

        TEST_HASH_FUNCTION(cn_lite_slow_hash_v0, CN_LITE_SLOW_HASH_V0);
        TEST_HASH_FUNCTION(cn_lite_slow_hash_v1, CN_LITE_SLOW_HASH_V1);
        TEST_HASH_FUNCTION(cn_lite_slow_hash_v2, CN_LITE_SLOW_HASH_V2);

        std::cout << std::endl;

        TEST_HASH_FUNCTION(cn_dark_slow_hash_v0, CN_DARK_SLOW_HASH_V0);
        TEST_HASH_FUNCTION(cn_dark_slow_hash_v1, CN_DARK_SLOW_HASH_V1);
        TEST_HASH_FUNCTION(cn_dark_slow_hash_v2, CN_DARK_SLOW_HASH_V2);

        std::cout << std::endl;

        TEST_HASH_FUNCTION(cn_dark_lite_slow_hash_v0, CN_DARK_LITE_SLOW_HASH_V0);
        TEST_HASH_FUNCTION(cn_dark_lite_slow_hash_v1, CN_DARK_LITE_SLOW_HASH_V1);
        TEST_HASH_FUNCTION(cn_dark_lite_slow_hash_v2, CN_DARK_LITE_SLOW_HASH_V2);
        
        std::cout << std::endl;

        TEST_HASH_FUNCTION(cn_turtle_slow_hash_v0, CN_TURTLE_SLOW_HASH_V0);
        TEST_HASH_FUNCTION(cn_turtle_slow_hash_v1, CN_TURTLE_SLOW_HASH_V1);
        TEST_HASH_FUNCTION(cn_turtle_slow_hash_v2, CN_TURTLE_SLOW_HASH_V2);

        std::cout << std::endl;

        TEST_HASH_FUNCTION(cn_turtle_lite_slow_hash_v0, CN_TURTLE_LITE_SLOW_HASH_V0);
        TEST_HASH_FUNCTION(cn_turtle_lite_slow_hash_v1, CN_TURTLE_LITE_SLOW_HASH_V1);
        TEST_HASH_FUNCTION(cn_turtle_lite_slow_hash_v2, CN_TURTLE_LITE_SLOW_HASH_V2);

        std::cout << std::endl;

        for (uint64_t height = 0; height <= 8192; height += 512)
        {
            TEST_HASH_FUNCTION_WITH_HEIGHT(cn_soft_shell_slow_hash_v0, CN_SOFT_SHELL_V0[height / 512], height);
        }

        std::cout << std::endl;

        for (uint64_t height = 0; height <= 8192; height += 512)
        {
            TEST_HASH_FUNCTION_WITH_HEIGHT(cn_soft_shell_slow_hash_v1, CN_SOFT_SHELL_V1[height / 512], height);
        }

        std::cout << std::endl;

        for (uint64_t height = 0; height <= 8192; height += 512)
        {
            TEST_HASH_FUNCTION_WITH_HEIGHT(cn_soft_shell_slow_hash_v2, CN_SOFT_SHELL_V2[height / 512], height);
        }

        if (o_benchmark)
        {
            std::cout <<  "\nPerformance Tests: Please wait, this may take a while depending on your system...\n\n";

            benchmarkUnderivePublicKey();
            benchmarkGenerateKeyDerivation();

            BENCHMARK(cn_slow_hash_v0, o_iterations);
            BENCHMARK(cn_slow_hash_v1, o_iterations);
            BENCHMARK(cn_slow_hash_v2, o_iterations);

            BENCHMARK(cn_lite_slow_hash_v0, o_iterations);
            BENCHMARK(cn_lite_slow_hash_v1, o_iterations);
            BENCHMARK(cn_lite_slow_hash_v2, o_iterations);

            BENCHMARK(cn_dark_slow_hash_v0, o_iterations);
            BENCHMARK(cn_dark_slow_hash_v1, o_iterations);
            BENCHMARK(cn_dark_slow_hash_v2, o_iterations);

            BENCHMARK(cn_dark_lite_slow_hash_v0, o_iterations);
            BENCHMARK(cn_dark_lite_slow_hash_v1, o_iterations);
            BENCHMARK(cn_dark_lite_slow_hash_v2, o_iterations);

            BENCHMARK(cn_turtle_slow_hash_v0, o_iterations_long);
            BENCHMARK(cn_turtle_slow_hash_v1, o_iterations_long);
            BENCHMARK(cn_turtle_slow_hash_v2, o_iterations_long);

            BENCHMARK(cn_turtle_lite_slow_hash_v0, o_iterations_long);
            BENCHMARK(cn_turtle_lite_slow_hash_v1, o_iterations_long);
            BENCHMARK(cn_turtle_lite_slow_hash_v2, o_iterations_long);
        }
    }
    catch (std::exception& e)
    {
        std::cout << "Something went terribly wrong...\n" << e.what() << "\n\n";
    }
}
