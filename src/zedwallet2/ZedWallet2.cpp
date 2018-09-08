// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#include <tuple>
#include <WalletBackend/WalletBackend.h>

int main()
{
    std::string mode;

    std::cout << "Mode? (read/write): ";

    std::getline(std::cin, mode);

    bool read = false;

    if (mode == "read")
    {
        read = true;
    }

    std::string walletName = "test.wallet";
    std::string walletPass = "password";
    std::string daemonHost = "127.0.0.1";
    uint16_t daemonPort = 11898;

    std::tuple<WalletError, WalletBackend> maybeWallet
        = read ? WalletBackend::openWallet(walletName, walletPass, daemonHost, daemonPort)
               : WalletBackend::createWallet(walletName, walletPass, daemonHost, daemonPort);

    WalletError error = std::get<0>(maybeWallet);
    WalletBackend wallet = std::get<1>(maybeWallet);

    if (!error)
    {
        if (read)
        {
            std::cout << "Wallet successfully opened!" << std::endl;
        }
        else
        {
            std::cout << "Wallet successfully created!" << std::endl;
        }
    }
    else
    {
        if (read)
        {
            std::cout << "Failed to open wallet, error code: " << error << std::endl;
        }
        else
        {
            std::cout << "Failed to create wallet, error code: " << error << std::endl;
        }
    }

    return 0;
}
