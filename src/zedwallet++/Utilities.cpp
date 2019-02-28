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
#include <zedwallet++/PasswordContainer.h>

namespace ZedUtilities
{

void confirmPassword(const std::shared_ptr<WalletBackend> walletBackend, const std::string msg)
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

bool confirm(const std::string &msg) { return confirm(msg, true); }

/* defaultToYes = what value we return on hitting enter, i.e. the "expected"
   workflow */
bool confirm(const std::string &msg, const bool defaultToYes)
{
    /* In unix programs, the upper case letter indicates the default, for
       example when you hit enter */
    const std::string prompt = defaultToYes ? " (Y/n): " : " (y/N): ";

    while (true)
    {
        std::cout << InformationMsg(msg + prompt);

        std::string answer;
        std::getline(std::cin, answer);

        const char c = ::tolower(answer[0]);

        switch (c)
        {
        /* Lets people spam enter / choose default value */
        case '\0':
            return defaultToYes;
        case 'y':
            return true;
        case 'n':
            return false;
        }

        std::cout << WarningMsg("Bad input: ") << InformationMsg(answer) << WarningMsg(" - please enter either Y or N.")
                  << std::endl;
    }
}

std::string unixTimeToDate(const uint64_t timestamp)
{
    const std::time_t time = timestamp;
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%F %R", std::localtime(&time));
    return std::string(buffer);
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
                  << InformationMsg("Hit enter for the sub-optimal default ") << InformationMsg("of zero: ");

        std::string stringHeight;

        std::getline(std::cin, stringHeight);

        /* Remove commas so user can enter height as e.g. 200,000 */
        removeCharFromString(stringHeight, ',');

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
            std::cout << WarningMsg("Failed to parse height - input is not ") << WarningMsg("a number!") << std::endl
                      << std::endl;
        }
    }
}

/* Erases all instances of c from the string. E.g. 2,000,000 becomes 2000000 */
void removeCharFromString(std::string &str, const char c)
{
    str.erase(std::remove(str.begin(), str.end(), c), str.end());
}

/* Trims any whitespace from left and right */
void trim(std::string &str)
{
    rightTrim(str);
    leftTrim(str);
}

void leftTrim(std::string &str)
{
    std::string whitespace = " \t\n\r\f\v";

    str.erase(0, str.find_first_not_of(whitespace));
}

void rightTrim(std::string &str)
{
    std::string whitespace = " \t\n\r\f\v";

    str.erase(str.find_last_not_of(whitespace) + 1);
}

/* Checks if str begins with substring */
bool startsWith(const std::string &str, const std::string &substring) { return str.rfind(substring, 0) == 0; }

std::vector<std::string> split(const std::string &str, char delim = ' ')
{
    std::vector<std::string> cont;
    std::stringstream ss(str);
    std::string token;

    while (std::getline(ss, token, delim))
    {
        cont.push_back(token);
    }

    return cont;
}

bool parseDaemonAddressFromString(std::string &host, uint16_t &port, const std::string address)
{
    std::vector<std::string> parts = split(address, ':');

    if (parts.empty())
    {
        return false;
    }
    else if (parts.size() >= 2)
    {
        try
        {
            host = parts.at(0);
            port = std::stoi(parts.at(1));
            return true;
        }
        catch (const std::invalid_argument &)
        {
            return false;
        }
    }

    host = parts.at(0);
    port = CryptoNote::RPC_DEFAULT_PORT;

    return true;
}

} // namespace ZedUtilities
