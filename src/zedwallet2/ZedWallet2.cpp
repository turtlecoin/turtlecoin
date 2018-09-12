// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#include <tuple>
#include <WalletBackend/WalletBackend.h>
#include <Common/StringTools.h>

int main()
{
    std::string walletName = "test.wallet";
    std::string walletPass = "password";
    std::string daemonHost = "public.turtlenode.io";
    std::string seed = "biggest yields peeled pawnshop godfather likewise hickory queen exit trying buying island wagtail vitals lucky theatrics dewdrop licks update pivot digit foes ensign estate queen";
    std::string address = "TRTLv2Fyavy8CXG8BPEbNeCHFZ1fuDCYCZ3vW5H5LXN4K2M2MHUpTENip9bbavpHvvPwb4NDkBWrNgURAd5DB38FHXWZyoBh4wW";

    Crypto::SecretKey privateSpendKey;
    Crypto::SecretKey privateViewKey;

    Common::podFromHex("b8e348a89ad416267ce9cf947e6c0d4b269cfa901aa2da4bd25438893b9f0a02", privateSpendKey.data);
    Common::podFromHex("9cf71be446b123e930fc0fc9e27ce5730386b93e30c7eb1747834d56b108ed08", privateViewKey.data);

    uint16_t daemonPort = 11898;

    WalletBackend wallet;
    WalletError error;

    std::string selection;

    std::cout << "Selection? (open/create/seed/keys/view): ";

    std::getline(std::cin, selection);

    if (selection == "open")
    {
        std::tie(error, wallet) = WalletBackend::openWallet(walletName, walletPass, daemonHost, daemonPort);
    }
    else if (selection == "create")
    {
        std::tie(error, wallet) = WalletBackend::createWallet(walletName, walletPass, daemonHost, daemonPort);
    }
    else if (selection == "seed")
    {
        std::tie(error, wallet) = WalletBackend::importWalletFromSeed(seed, walletName, walletPass, 0, daemonHost, daemonPort);
    }
    else if (selection == "keys")
    {
        std::tie(error, wallet) = WalletBackend::importWalletFromKeys(privateSpendKey, privateViewKey, walletName, walletPass, 0, daemonHost, daemonPort);
    }
    else if (selection == "view")
    {
        std::tie(error, wallet) = WalletBackend::importViewWallet(privateSpendKey, address, walletName, walletPass, 0, daemonHost, daemonPort);
    }
    else
    {
        std::cout << "Bad input." << std::endl;
        return 1;
    }

    if (!error)
    {
        std::cout << "Operation succeeded!" << std::endl;
    }
    else
    {
        std::cout << "Operation failed, error code: " <<  error << std::endl;
    }

    return 0;
}
