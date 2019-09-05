#include <iostream>

#include <config/CliHeader.h>
#include <common/ConsoleTools.h>
#include <common/FileSystemShim.h>
#include <cxxopts.hpp>
#include <utilities/PasswordContainer.h>
#include <utilities/ColouredMsg.h>
#include <walletbackend/WalletBackend.h>

int main(int argc, char **argv)
{
    std::string walletName;
    std::string walletPass;

    bool help;
    bool version;

    cxxopts::Options options(argv[0], CryptoNote::getProjectCLIHeader());

    options.add_options("Core")(
        "h,help", "Display this help message", cxxopts::value<bool>(help)->implicit_value("true"))

        ("v,version",
         "Output software version information",
         cxxopts::value<bool>(version)->default_value("false")->implicit_value("true"));

    options.add_options("Wallet")(
        "w,wallet-file", "Open the wallet <file>", cxxopts::value<std::string>(walletName), "<file>")

        ("p,password",
         "Use the password <pass> to open the wallet",
         cxxopts::value<std::string>(walletPass),
         "<pass>");

    bool walletGiven = false;
    bool passGiven = false;

    try
    {
        const auto result = options.parse(argc, argv);

        walletGiven = result.count("wallet-file") != 0;

        /* We could check if the string is empty, but an empty password is valid */
        passGiven = result.count("password") != 0;
    }
    catch (const cxxopts::OptionException &e)
    {
        std::cout << "Error: Unable to parse command line argument options: " << e.what() << std::endl << std::endl;
        std::cout << options.help({}) << std::endl;
        exit(1);
    }

    if (help) // Do we want to display the help message?
    {
        std::cout << options.help({}) << std::endl;
        exit(0);
    }
    else if (version) // Do we want to display the software version?
    {
        std::cout << CryptoNote::getProjectCLIHeader() << std::endl;
        exit(0);
    }

    bool initial = true;

    while (true)
    {
        std::string filename;

        while (true)
        {
            if (!walletGiven || !initial)
            {
                std::cout << InformationMsg("What is the name of the wallet ") << InformationMsg("you want to upgrade?: ");

                std::getline(std::cin, walletName);
            }

            const std::string walletFileName = walletName + ".wallet";

            initial = false;

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

        if (!passGiven)
        {
            Tools::PasswordContainer pwdContainer;
            pwdContainer.read_password(false, "Enter wallet password: ");
            walletPass = pwdContainer.password();
        }

        passGiven = false;

        std::cout << InformationMsg("Upgrading...") << std::endl;

        const auto [error, wallet] = WalletBackend::openWallet(filename, walletPass, "DEADBEEF", 0, 1);

        if (!error)
        {
            std::cout << SuccessMsg("Done!") << std::endl;

            std::cout << InformationMsg("You can now open your wallet in proton, zedwallet-beta or wallet-api.") << std::endl;
            break;
        }
        else
        {
            std::cout << WarningMsg("Sorry, we were unable to upgrade your wallet.. Are you sure this is a wallet file?") << std::endl;
            std::cout << WarningMsg(error) << std::endl;
            std::cout << WarningMsg("Or, maybe you just typed your password wrong.") << std::endl;
            std::cout << InformationMsg("Try again.\n") << std::endl;
        }
    }

    if (Common::Console::isConsoleTty())
    {
        std::cout << InformationMsg("Hit enter to exit: ");

        std::string dummy;

        std::getline(std::cin, dummy);
    }

    return 0;
}
