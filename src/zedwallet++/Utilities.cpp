// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

//////////////////////////////////
#include <zedwallet++/Utilities.h>
//////////////////////////////////

#include <cmath>

#include <config/WalletConfig.h>

#include <fstream>

#include <iostream>

#include <Utilities/ColouredMsg.h>
#include <Utilities/String.h>

#include <zedwallet++/PasswordContainer.h>

namespace ZedUtilities
{

void confirmPassword(
    const std::shared_ptr<WalletBackend> walletBackend,
    const std::string msg)
{
    const std::string currentPassword = walletBackend->getWalletPassword();

    /* Password container requires an rvalue, we don't want to wipe our current
       pass so copy it into a tmp string and std::move that instead */
    std::string tmpString = currentPassword;

    Tools::PasswordContainer pwdContainer(std::move(tmpString));

    while (!pwdContainer.read_and_validate(msg))
    {
        std::cout << WarningMsg("Incorrect password! Try again.") << std::endl;
    }
}

uint64_t getScanHeight()
{
    std::cout << "\n";

    while (true)
    {
        std::cout << InformationMsg("What height would you like to begin ")
                  << InformationMsg("scanning your wallet from?") << "\n\n"
                  << "This can greatly speed up the initial wallet "
                  << "scanning process."
                  << "\n\n"
                  << "If you do not know the exact height, "
                  << "err on the side of caution so transactions do not "
                  << "get missed."
                  << "\n\n"
                  << InformationMsg("Hit enter for the sub-optimal default ")
                  << InformationMsg("of zero: ");

        std::string stringHeight;

        std::getline(std::cin, stringHeight);

        /* Remove commas so user can enter height as e.g. 200,000 */
        Utilities::removeCharFromString(stringHeight, ',');

        if (stringHeight == "")
        {
            return 0;
        }

        try
        {
            return std::stoull(stringHeight);
        }
        catch (const std::out_of_range &)
        {
            std::cout << WarningMsg("Input is too large or too small!");
        }
        catch (const std::invalid_argument &)
        {
            std::cout << WarningMsg("Failed to parse height - input is not ")
                      << WarningMsg("a number!") << std::endl << std::endl;
        }
    }
}

} // namespace
