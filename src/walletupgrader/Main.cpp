#include <iostream>

#include <common/FileSystemShim.h>
#include <utilities/PasswordContainer.h>
#include <utilities/ColouredMsg.h>
#include <walletbackend/WalletBackend.h>

int main()
{
    while (true)
    {
        std::string filename;

        while (true)
        {
            std::cout << InformationMsg("What is the name of the wallet ") << InformationMsg("you want to upgrade?: ");

            std::string walletName;

            std::getline(std::cin, walletName);

            const std::string walletFileName = walletName + ".wallet";

            try
            {
                if (walletName == "")
                {
                    std::cout << WarningMsg("\nWallet name can't be blank! Try again.\n\n");
                }
                /* Allow people to enter wallet name with or without file extension */
                else if (fs::exists(walletName))
                {
                    filename = walletName;
                    break;
                }
                else if (fs::exists(walletFileName))
                {
                    filename = walletFileName;
                    break;
                }
                else
                {
                    std::cout << WarningMsg("\nA wallet with the filename ") << InformationMsg(walletName)
                              << WarningMsg(" or ") << InformationMsg(walletFileName) << WarningMsg(" doesn't exist!\n")
                              << "Ensure you entered your wallet name correctly.\n\n";
                }
            }
            catch (const fs::filesystem_error &)
            {
                std::cout << WarningMsg("\nInvalid wallet filename! Try again.\n\n");
            }
        }

        Tools::PasswordContainer pwdContainer;
        pwdContainer.read_password(false, "Enter wallet password: ");
        std::string walletPass = pwdContainer.password();

        std::cout << InformationMsg("Upgrading...") << std::endl;

        bool success = WalletBackend::tryUpgradeWalletFormat(
            filename, walletPass, "DEAFBEEF", 0
        );

        if (!success)
        {
            const auto [error, wallet] = WalletBackend::openWallet(filename, walletPass, "DEADBEEF", 0, 1);

            if (!error)
            {
                success = true;
            }
        }

        if (success)
        {
            std::cout << SuccessMsg("Done!") << std::endl;

            std::cout << InformationMsg("You can now open your wallet in proton, zedwallet-beta or wallet-api.") << std::endl;
            break;
        }
        else
        {
            std::cout << WarningMsg("Sorry, we were unable to upgrade your wallet.. Are you sure this is a wallet file?") << std::endl;
            std::cout << InformationMsg("Try again.\n") << std::endl;
        }
    }

    std::cout << InformationMsg("Hit enter to exit: ");

    std::string dummy;

    std::getline(std::cin, dummy);

    return 0;
}
