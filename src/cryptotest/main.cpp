// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "CryptoNote.h"
#include "CryptoTypes.h"
#include "common/StringTools.h"
#include "crypto/crypto.h"
#include "crypto/multisig.h"

#include <assert.h>
#include <chrono>
#include <config/CliHeader.h>
#include <cxxopts.hpp>
#include <iostream>

#define PERFORMANCE_ITERATIONS 1000
#define PERFORMANCE_ITERATIONS_LONG_MULTIPLIER 10

using namespace Crypto;
using namespace CryptoNote;

const std::string INPUT_DATA = "0100fb8e8ac805899323371bb790db19218afd8db8e3755d8b90f39b3d5506a9abce4fa912244500000000e"
                               "e8146d49fa93ee724deb57d12cbc6c6f3b924d946127c7a97418f9348828f0f02";

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

const std::string CHUKWA_V1 = "c0dad0eeb9c52e92a1c3aa5b76a3cb90bd7376c28dce191ceeb1096e3a390d2e";

const std::string CHUKWA_V2 = "3578c135261366a7bac407b8c0ff50f3ad96f096ec2813e9644e6e77a43f803d";

const std::string CN_SOFT_SHELL_V0[] = {"5e1891a15d5d85c09baf4a3bbe33675cfa3f77229c8ad66c01779e590528d6d3",
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
                                        "5e1891a15d5d85c09baf4a3bbe33675cfa3f77229c8ad66c01779e590528d6d3"};

const std::string CN_SOFT_SHELL_V1[] = {"ae7f864a7a2f2b07dcef253581e60a014972b9655a152341cb989164761c180a",
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
                                        "ae7f864a7a2f2b07dcef253581e60a014972b9655a152341cb989164761c180a"};

const std::string CN_SOFT_SHELL_V2[] = {"b2172ec9466e1aee70ec8572a14c233ee354582bcb93f869d429744de5726a26",
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
                                        "b2172ec9466e1aee70ec8572a14c233ee354582bcb93f869d429744de5726a26"};

static inline bool CompareHashes(const Hash leftHash, const std::string right)
{
    Hash rightHash = Hash();
    if (!Common::podFromHex(right, rightHash))
    {
        return false;
    }

    return (leftHash == rightHash);
}

/* Hacky way to check if we're testing a v1 hash and thus should skip data
   < 43 bytes */
bool need43BytesOfData(std::string hashFunctionName)
{
    return hashFunctionName.find("v1") != std::string::npos;
}

/* Bit of hackery so we can get the variable name of the passed in function.
   This way we can print the test we are currently performing. */
#define TEST_HASH_FUNCTION(hashFunction, expectedOutput) \
    testHashFunction(hashFunction, expectedOutput, #hashFunction, -1)

#define TEST_HASH_FUNCTION_WITH_HEIGHT(hashFunction, expectedOutput, height) \
    testHashFunction(hashFunction, expectedOutput, #hashFunction, height, height)

template<typename T, typename... Args>
void testHashFunction(
    T hashFunction,
    std::string expectedOutput,
    std::string hashFunctionName,
    int64_t height,
    Args &&... args)
{
    const BinaryArray &rawData = Common::fromHex(INPUT_DATA);

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
    if (!CompareHashes(hash, expectedOutput))
    {
        std::cout << "Hashes are not equal!\n"
                  << "Expected: " << expectedOutput << "\nActual: " << hash << "\nTerminating.";

        exit(1);
    }
}

/* Bit of hackery so we can get the variable name of the passed in function.
   This way we can print the test we are currently performing. */
#define BENCHMARK(hashFunction, iterations) benchmark(hashFunction, #hashFunction, iterations)

template<typename T> void benchmark(T hashFunction, std::string hashFunctionName, uint64_t iterations)
{
    const BinaryArray &rawData = Common::fromHex(INPUT_DATA);

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
              << (iterations / std::chrono::duration_cast<std::chrono::seconds>(elapsedTime).count()) << " H/s\n";
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
    const auto timePerDerivation =
        std::chrono::duration_cast<std::chrono::microseconds>(elapsedTime).count() / loopIterations;

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

    const auto timePerDerivation =
        std::chrono::duration_cast<std::chrono::microseconds>(elapsedTime).count() / loopIterations;

    std::cout << "Time to perform generateKeyDerivation: " << timePerDerivation / 1000.0 << " ms" << std::endl;
}

void TestDeterministicSubwalletCreation(
    const std::string baseSpendKey,
    const uint64_t subWalletIndex,
    const std::string expectedSpendKey)
{
    Crypto::SecretKey f_baseSpendKey;

    if (!Common::podFromHex(baseSpendKey, f_baseSpendKey))
    {
        std::cout << "Could not decode base private spend key!\nTerminating...";

        exit(1);
    }

    Crypto::SecretKey f_expectedSpendKey;

    if (!Common::podFromHex(expectedSpendKey, f_expectedSpendKey))
    {
        std::cout << "Could not decode expected private spend key!\nTerminating...";

        exit(1);
    }

    const auto [subwalletPrivateKey, subwalletPublicKey] =
        Crypto::generate_deterministic_subwallet_keys(f_baseSpendKey, subWalletIndex);

    if (subwalletPrivateKey != f_expectedSpendKey)
    {
        std::cout << "Could not deterministically create subwallet spend keys!\n"
                  << "Expected: " << expectedSpendKey << "\nActual: " << subwalletPrivateKey << "\nTerminating.";

        exit(1);
    }
}

int main(int argc, char **argv)
{
    bool o_help, o_version, o_benchmark;
    int o_iterations;

    cxxopts::Options options(argv[0], getProjectCLIHeader());

    options.add_options("Core")(
        "h,help", "Display this help message", cxxopts::value<bool>(o_help)->implicit_value("true"))(
        "v,version",
        "Output software version information",
        cxxopts::value<bool>(o_version)->default_value("false")->implicit_value("true"));

    options.add_options("Performance Testing")(
        "b,benchmark",
        "Run quick performance benchmark",
        cxxopts::value<bool>(o_benchmark)->default_value("false")->implicit_value("true"))(
        "i,iterations",
        "The number of iterations for the benchmark test. Minimum of 1,000 iterations required.",
        cxxopts::value<int>(o_iterations)->default_value(std::to_string(PERFORMANCE_ITERATIONS)),
        "#");

    try
    {
        auto result = options.parse(argc, argv);
    }
    catch (const cxxopts::OptionException &e)
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
        std::cout << std::endl
                  << "Error: The number of --iterations should be at least 1,000 for reasonable accuracy" << std::endl;
        exit(1);
    }

    int o_iterations_long = o_iterations * PERFORMANCE_ITERATIONS_LONG_MULTIPLIER;

    try
    {
        std::cout << getProjectCLIHeader() << std::endl;

        std::cout << std::endl << "Test Crypto Primitives" << std::endl << std::endl;

        {
            std::cout << "Crypto::crypto_ops::prepareRingSignatures: ";

            Crypto::Hash txPrefixHash;

            Common::podFromHex("b542df5b6e7f5f05275c98e7345884e2ac726aeeb07e03e44e0389eb86cd05f0", txPrefixHash);

            Crypto::KeyImage keyImage("6865866ed8a25824e042e21dd36e946836b58b03366e489aecf979f444f599b0");

            std::vector<Crypto::PublicKey> publicKeys;

            publicKeys.push_back(Crypto::PublicKey("492390897da1cabd3886e3eff43ad1d04aa510a905bec0acd31a0a2f260e7862"));

            publicKeys.push_back(Crypto::PublicKey("7644ccb5410cca2be18b033e5f7497aeeeafd1d8f317f29cba4803e4306aa402"));

            publicKeys.push_back(Crypto::PublicKey("bb9a956ffdf8159ad69474e6b0811316c44a17a540d5e39a44642d4d933a6460"));

            publicKeys.push_back(Crypto::PublicKey("e1cd9ccdfdf2b3a45ac2cfd1e29185d22c185742849f52368c3cdd1c0ce499c0"));

            Crypto::SecretKey privateEmpheremal("73a8e577d58f7c11992201d4014ac7eef39c1e9f6f6d78673103de60a0c3240b");

            const auto [success, signatures, k] = Crypto::crypto_ops::prepareRingSignatures(txPrefixHash, keyImage, publicKeys, 3);

            if (!success)
            {
                std::cout << "failed" << std::endl;

                exit(1);
            }

            std::cout << "passed" << std::endl;
        }

        {
            std::cout << "Crypto::crypto_ops::generateRingSignatures: ";

            Crypto::Hash txPrefixHash;

            Common::podFromHex("b542df5b6e7f5f05275c98e7345884e2ac726aeeb07e03e44e0389eb86cd05f0", txPrefixHash);

            Crypto::KeyImage keyImage("6865866ed8a25824e042e21dd36e946836b58b03366e489aecf979f444f599b0");

            std::vector<Crypto::PublicKey> publicKeys;

            publicKeys.push_back(Crypto::PublicKey("492390897da1cabd3886e3eff43ad1d04aa510a905bec0acd31a0a2f260e7862"));

            publicKeys.push_back(Crypto::PublicKey("7644ccb5410cca2be18b033e5f7497aeeeafd1d8f317f29cba4803e4306aa402"));

            publicKeys.push_back(Crypto::PublicKey("bb9a956ffdf8159ad69474e6b0811316c44a17a540d5e39a44642d4d933a6460"));

            publicKeys.push_back(Crypto::PublicKey("e1cd9ccdfdf2b3a45ac2cfd1e29185d22c185742849f52368c3cdd1c0ce499c0"));

            Crypto::SecretKey privateEmpheremal("73a8e577d58f7c11992201d4014ac7eef39c1e9f6f6d78673103de60a0c3240b");

            const auto [success, signatures] =
                Crypto::crypto_ops::generateRingSignatures(txPrefixHash, keyImage, publicKeys, privateEmpheremal, 3);

            if (!success)
            {
                std::cout << "failed" << std::endl;

                exit(1);
            }

            bool check = Crypto::crypto_ops::checkRingSignature(txPrefixHash, keyImage, publicKeys, signatures);

            if (!check)
            {
                std::cout << "failed" << std::endl;

                exit(1);
            }

            std::cout << "passed" << std::endl;
        }

        {
            std::cout << "Crypto::generate_deterministic_subwallet_keys: ";

            TestDeterministicSubwalletCreation(
                "dd0c02d3202634821b4d9d91b63d919725f5c3e97e803f3512e52fb0dc2aab0c",
                0,
                "dd0c02d3202634821b4d9d91b63d919725f5c3e97e803f3512e52fb0dc2aab0c");
            TestDeterministicSubwalletCreation(
                "dd0c02d3202634821b4d9d91b63d919725f5c3e97e803f3512e52fb0dc2aab0c",
                1,
                "c55cbe4fd1c49dca5958fa1c7b9212c2dbf3fd5bfec84de741d434056e298600");
            TestDeterministicSubwalletCreation(
                "dd0c02d3202634821b4d9d91b63d919725f5c3e97e803f3512e52fb0dc2aab0c",
                2,
                "9813c40428ed9b380a2f72bac1374a9d3852a974b0527e003cbc93afab764d01");
            TestDeterministicSubwalletCreation(
                "dd0c02d3202634821b4d9d91b63d919725f5c3e97e803f3512e52fb0dc2aab0c",
                64,
                "29c2afed13271e2bb3321c2483356fd8798f2709af4de3906b6627ec71727108");
            TestDeterministicSubwalletCreation(
                "dd0c02d3202634821b4d9d91b63d919725f5c3e97e803f3512e52fb0dc2aab0c",
                65,
                "0c6b5fff72260832558e35c38e690072503211af065056862288dc7fd992350a");

            std::cout << "passed" << std::endl;
        }

        std::cout << std::endl << "Test Multisig Primitives" << std::endl << std::endl;

        {
            std::cout << "Crypto::Multisig::calculate_multisig_private_keys: ";

            Crypto::SecretKey privateKey("a0ba0cae34ce1133b9cb658e5d0a56440608622a64562ac360907a2c68ea130d");

            std::vector<Crypto::PublicKey> publicKeys;

            publicKeys.push_back(Crypto::PublicKey("ba719ff6486ae5ab5ea0c7e05f6b42468f898bd366f83a4d165e396c1f7c5eec"));

            publicKeys.push_back(Crypto::PublicKey("fd524a5384bf5044feeb61f19866e11f74b8dbf5e7d050238046b04289a31849"));

            std::vector<Crypto::SecretKey> multisigKeys =
                Crypto::Multisig::calculate_multisig_private_keys(privateKey, publicKeys);

            if (multisigKeys[0] != Crypto::SecretKey("ca67bdeba4cc489c86b0e6be24ed86ee75fd7e4caaf6566ea3b241946f40f901")
                || multisigKeys[1]
                       != Crypto::SecretKey("98c2625a77504c46fb4d83bdf2c5dee505d4e3d0d30005bac636b0d49f90420f"))
            {
                std::cout << "failed" << std::endl;

                exit(1);
            }

            std::cout << "passed" << std::endl;
        }

        {
            std::cout << "Crypto::Multisig::calculate_shared_public_key: ";

            std::vector<Crypto::PublicKey> publicKeys;

            publicKeys.push_back(Crypto::PublicKey("6bce43e0d797b9ee674db41c173f9b147fab6841fed36e97d434bd7c6f5b81d5"));

            publicKeys.push_back(Crypto::PublicKey("ba719ff6486ae5ab5ea0c7e05f6b42468f898bd366f83a4d165e396c1f7c5eec"));

            Crypto::PublicKey sharedPublicKey = Crypto::Multisig::calculate_shared_public_key(publicKeys);

            if (sharedPublicKey
                != Crypto::PublicKey("caa8f9aaf673ff2c055025942eeefde720a71281420ec8c42f0a817225db032b"))
            {
                std::cout << "failed" << std::endl;

                exit(1);
            }

            std::cout << "passed" << std::endl;
        }

        {
            std::cout << "Crypto::Multisig::calculate_shared_private_key: ";

            std::vector<Crypto::SecretKey> secretKeys;

            secretKeys.push_back(Crypto::SecretKey("01d85bf9ce5583c7a1039f2c2695cb562bf1ea97636bbaf9051af01dddc89e0b"));

            secretKeys.push_back(Crypto::SecretKey("650110a79f0353624f0fa14aaaf8c5af405ddb009c3127366e5b8591ecec9704"));

            Crypto::SecretKey sharedPrivateKey = Crypto::Multisig::calculate_shared_private_key(secretKeys);

            if (sharedPrivateKey
                != Crypto::SecretKey("7905764354f6c3d11a7648d4f193b2f16b4ec698ff9ce12f747575afc9b53600"))
            {
                std::cout << "failed" << std::endl;

                exit(1);
            }

            std::cout << "passed" << std::endl;
        }

        {
            std::cout << "Crypto::Multisig::restore_key_image: ";

            Crypto::PublicKey publicEphemeral("e1cd9ccdfdf2b3a45ac2cfd1e29185d22c185742849f52368c3cdd1c0ce499c0");

            Crypto::KeyDerivation derivation("9475ebaa9f869b06d967aa0ca09d1632f4b8a383211c8a66e39021bc04d80fc4");

            std::vector<Crypto::KeyImage> partialKeyImages;

            partialKeyImages.push_back(
                Crypto::KeyImage("f67f9a1a525d9f34386c8d8f2bfebe15e653b7fbbf561da8531eedbf5dd06317"));

            partialKeyImages.push_back(
                Crypto::KeyImage("b04a322530870398ce1f1bd4df2e40155425a8ed45fb8f4637f22c648cbac2f2"));

            Crypto::KeyImage keyImage =
                Crypto::Multisig::restore_key_image(publicEphemeral, derivation, 2, partialKeyImages);

            if (keyImage != Crypto::KeyImage("6865866ed8a25824e042e21dd36e946836b58b03366e489aecf979f444f599b0"))
            {
                std::cout << "failed" << std::endl;

                exit(1);
            }

            std::cout << "passed" << std::endl;
        }

        {
            std::cout << "Crypto::Multisig::generate_partial_signing_key: ";

            Crypto::Signature signature("d3b4f642eb7049e00b17130ec95d47e878c756a205766418687667fe4877920500000000000000"
                                        "00000000000000000000000000000000000000000000000000");

            Crypto::SecretKey privateSpendKey("a0ba0cae34ce1133b9cb658e5d0a56440608622a64562ac360907a2c68ea130d");

            Crypto::SecretKey partialKey = Crypto::Multisig::generate_partial_signing_key(signature, privateSpendKey);

            if (partialKey != Crypto::SecretKey("bea03f1dcdc3a6375d883afa86f88e4a43606bcc2b0c9b00e313813f7436ef03"))
            {
                std::cout << "failed" << std::endl;

                exit(1);
            }

            std::cout << "passed" << std::endl;
        }

        {
            std::cout << "Crypto::Multisig::restore_ring_signatures: ";

            Crypto::KeyDerivation derivation("9475ebaa9f869b06d967aa0ca09d1632f4b8a383211c8a66e39021bc04d80fc4");

            std::vector<Crypto::SecretKey> partialSigningKeys;

            partialSigningKeys.push_back(
                Crypto::SecretKey("bea03f1dcdc3a6375d883afa86f88e4a43606bcc2b0c9b00e313813f7436ef03"));

            partialSigningKeys.push_back(
                Crypto::SecretKey("14c09b9e8186a405f66fcd695c7ca675018f355eb3e28c240e3e647913f3c506"));

            Crypto::EllipticCurveScalar k("80bd5c68a280c2071c0a11be82e83c0fd2539660b21f7d9ff54a654f2a73c40d");

            std::vector<Crypto::Signature> signatures;

            signatures.push_back(Crypto::Signature("719afc6be33058758d3aea7c382c6bf9340b62b2297fb93b42d0c984af8c0f0b08d"
                                                   "7973924dc379e9b75ae5135ed6f40efc7418d597eaabdb69ebbb2b7630b05"));

            signatures.push_back(Crypto::Signature("128bfd98170ea31dfdcc2214e14e66a08e4f66d581d2317ab0c583e4573c9103ec1"
                                                   "1bc5dd9e7f734b3f0fbd3c29eddea102275b9f871cb96b658ca0787261607"));

            signatures.push_back(Crypto::Signature("ef602f4a07c2b643b456d5587f682a7d44cb360cd83bdb2d176f3ad687027c0bf38"
                                                   "8ab6dbf91dcec2fdfab865dd065c02905f1fa6c7b778cb9773cfd839bd300"));

            signatures.push_back(Crypto::Signature("d3b4f642eb7049e00b17130ec95d47e878c756a205766418687667fe48779205000"
                                                   "0000000000000000000000000000000000000000000000000000000000000"));

            const auto [success, restoredSignatures] =
                Crypto::Multisig::restore_ring_signatures(derivation, 2, partialSigningKeys, 3, k, signatures);

            if (!success)
            {
                std::cout << "failed" << std::endl;

                exit(1);
            }

            Crypto::Hash txPrefixHash;

            Common::podFromHex("b542df5b6e7f5f05275c98e7345884e2ac726aeeb07e03e44e0389eb86cd05f0", txPrefixHash);

            Crypto::KeyImage keyImage("6865866ed8a25824e042e21dd36e946836b58b03366e489aecf979f444f599b0");

            std::vector<Crypto::PublicKey> publicKeys;

            publicKeys.push_back(Crypto::PublicKey("492390897da1cabd3886e3eff43ad1d04aa510a905bec0acd31a0a2f260e7862"));

            publicKeys.push_back(Crypto::PublicKey("7644ccb5410cca2be18b033e5f7497aeeeafd1d8f317f29cba4803e4306aa402"));

            publicKeys.push_back(Crypto::PublicKey("bb9a956ffdf8159ad69474e6b0811316c44a17a540d5e39a44642d4d933a6460"));

            publicKeys.push_back(Crypto::PublicKey("e1cd9ccdfdf2b3a45ac2cfd1e29185d22c185742849f52368c3cdd1c0ce499c0"));

            bool check = Crypto::crypto_ops::checkRingSignature(txPrefixHash, keyImage, publicKeys, restoredSignatures);

            if (!check)
            {
                std::cout << "failed" << std::endl;

                exit(1);
            }

            std::cout << "passed" << std::endl;
        }

        std::cout << std::endl << "Input: " << INPUT_DATA << std::endl << std::endl;

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

        TEST_HASH_FUNCTION(chukwa_slow_hash_v1, CHUKWA_V1);
        TEST_HASH_FUNCTION(chukwa_slow_hash_v2, CHUKWA_V2);

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
            std::cout << "\nPerformance Tests: Please wait, this may take a while depending on your system...\n\n";

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

            BENCHMARK(chukwa_slow_hash_v1, o_iterations_long);
            BENCHMARK(chukwa_slow_hash_v2, o_iterations_long);
        }
    }
    catch (std::exception &e)
    {
        std::cout << "Something went terribly wrong...\n" << e.what() << "\n\n";
    }
}
