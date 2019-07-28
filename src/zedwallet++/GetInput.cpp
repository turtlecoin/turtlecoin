// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <algorithm>

/////////////////////////////////
#include <zedwallet++/GetInput.h>
/////////////////////////////////

#include "linenoise.hpp"

#include <config/WalletConfig.h>
#include <errors/ValidateParameters.h>
#include <utilities/ColouredMsg.h>
#include <utilities/FormatTools.h>
#include <utilities/Input.h>
#include <utilities/String.h>
#include <utilities/Utilities.h>
#include <zedwallet++/Commands.h>

/* Note: this is not portable, it only works with terminals that support ANSI
   codes (e.g., not Windows) - however! due to the way linenoise-cpp works,
   it will actually convert these codes for us to the windows equivalent. <3 */
std::string yellowANSIMsg(const std::string msg)
{
    const std::string CYELLOW = "\033[1;33m";
    const std::string RESET = "\033[0m";

    return CYELLOW + msg + RESET;
}

std::string getPrompt(std::shared_ptr<WalletBackend> walletBackend)
{
    const int promptLength = 20;
    const std::string extension = ".wallet";

    const std::string walletFileName = walletBackend->getWalletLocation();

    std::string walletName = walletFileName;

    /* Filename ends in .wallet, remove extension */
    if (std::equal(extension.rbegin(), extension.rend(), walletFileName.rbegin()))
    {
        const size_t extPos = walletFileName.find_last_of('.');

        walletName = walletFileName.substr(0, extPos);
    }

    const std::string shortName = walletName.substr(0, promptLength);

    return "[" + WalletConfig::ticker + " " + shortName + "]: ";
}

template<typename T> std::string getInput(const std::vector<T> &availableCommands, const std::string prompt)
{
    linenoise::SetCompletionCallback([availableCommands](const char *input, std::vector<std::string> &completions) {
        /* Convert to std::string */
        std::string c = input;

        for (const auto &command : availableCommands)
        {
            /* Does command begin with input? */
            if (command.commandName.compare(0, c.length(), c) == 0)
            {
                completions.push_back(command.commandName);
            }
        }
    });

    const std::string promptMsg = yellowANSIMsg(prompt);

    /* 256 max commands in the wallet command history */
    linenoise::SetHistoryMaxLen(256);

    /* The inputted command */
    std::string command;

    bool quit = linenoise::Readline(promptMsg.c_str(), command);

    /* User entered ctrl+c or similar */
    if (quit)
    {
        return "exit";
    }

    /* Remove any whitespace */
    Utilities::trim(command);

    if (command != "")
    {
        linenoise::AddHistory(command.c_str());
    }

    return command;
}

std::string getAddress(const std::string msg, const bool integratedAddressesAllowed, const bool cancelAllowed)
{
    while (true)
    {
        std::string address;

        std::cout << InformationMsg(msg);

        /* Fixes infinite looping when someone does a ctrl + c */
        if (!std::getline(std::cin, address))
        {
            return "cancel";
        }

        Utilities::trim(address);

        /* \n == no-op */
        if (address == "")
        {
            continue;
        }

        if (address == "cancel" && cancelAllowed)
        {
            return address;
        }

        if (Error error = validateAddresses({address}, integratedAddressesAllowed); error != SUCCESS)
        {
            std::cout << WarningMsg("Invalid address: ") << WarningMsg(error) << std::endl;
        }
        else
        {
            return address;
        }
    }
}

std::string getPaymentID(const std::string msg, const bool cancelAllowed)
{
    while (true)
    {
        std::cout << InformationMsg(msg)
                  << WarningMsg("\nWarning: If you were given a payment ID,\n"
                                "you MUST use it, or your funds may be lost!\n")
                  << "Hit enter for the default of no payment ID: ";

        std::string paymentID;

        /* Fixes infinite looping when someone does a ctrl + c */
        if (!std::getline(std::cin, paymentID))
        {
            return "cancel";
        }

        Utilities::trim(paymentID);

        if (paymentID == "cancel" && cancelAllowed)
        {
            return paymentID;
        }

        if (paymentID == "")
        {
            return paymentID;
        }

        /* Validate the payment ID */
        if (Error error = validatePaymentID(paymentID); error != SUCCESS)
        {
            std::cout << WarningMsg("Invalid payment ID: ") << WarningMsg(error) << std::endl;
        }
        else
        {
            return paymentID;
        }
    }
}

std::string getHash(const std::string msg, const bool cancelAllowed)
{
    while (true)
    {
        std::cout << InformationMsg(msg);

        std::string hash;

        /* Fixes infinite looping when someone does a ctrl + c */
        if (!std::getline(std::cin, hash))
        {
            return "cancel";
        }

        Utilities::trim(hash);

        if (hash == "cancel" && cancelAllowed)
        {
            return hash;
        }

        /* Validate the hash */
        if (Error error = validateHash(hash); error != SUCCESS)
        {
            std::cout << WarningMsg("Invalid hash: ") << WarningMsg(error) << std::endl;
        }
        else
        {
            return hash;
        }
    }
}

std::tuple<bool, uint64_t> getAmountToAtomic(const std::string msg, const bool cancelAllowed)
{
    while (true)
    {
        std::cout << InformationMsg(msg);

        std::string amountString;

        /* Fixes infinite looping when someone does a ctrl + c */
        if (!std::getline(std::cin, amountString))
        {
            return {false, 0};
        }

        /* \n == no-op */
        if (amountString == "")
        {
            continue;
        }

        Utilities::trim(amountString);

        /* If the user entered thousand separators, remove them */
        Utilities::removeCharFromString(amountString, ',');

        if (amountString == "cancel" && cancelAllowed)
        {
            return {false, 0};
        }

        /* Find the position of the decimal in the string */
        const uint64_t decimalPos = amountString.find_last_of('.');

        /* Get the length of the decimal part */
        const uint64_t decimalLength =
            decimalPos == std::string::npos ? 0 : amountString.substr(decimalPos + 1, amountString.length()).length();

        /* Can't send amounts with more decimal places than supported */
        if (decimalLength > WalletConfig::numDecimalPlaces)
        {
            std::stringstream stream;

            stream << CryptoNote::CRYPTONOTE_NAME << " transfers can have "
                   << "a max of " << WalletConfig::numDecimalPlaces << " decimal places.\n";

            std::cout << WarningMsg(stream.str());

            continue;
        }

        /* Remove the decimal place, so we can parse it as an atomic amount */
        Utilities::removeCharFromString(amountString, '.');

        /* Pad the string with 0's at the end, so 123 becomes 12300, so we
           can parse it as an atomic amount. 123.45 parses as 12345. */
        amountString.append(WalletConfig::numDecimalPlaces - decimalLength, '0');

        try
        {
            unsigned long long amount = std::stoull(amountString);

            if (amount < WalletConfig::minimumSend)
            {
                std::cout << WarningMsg("The minimum send allowed is ")
                          << WarningMsg(Utilities::formatAmount(WalletConfig::minimumSend)) << WarningMsg("!\n");
            }
            else
            {
                return {true, amount};
            }
        }
        catch (const std::out_of_range &)
        {
            std::cout << WarningMsg("Input is too large or too small!");
        }
        catch (const std::invalid_argument &)
        {
            std::cout << WarningMsg("Failed to parse amount! Ensure you entered "
                                    "the value correctly.\n");
        }
    }
}

std::tuple<std::string, uint16_t, bool> getDaemonAddress()
{
    while (true)
    {
        std::cout << InformationMsg("\nEnter the daemon address you want to use.\n"
                                    "You can omit the port, and it will default to ")
                  << InformationMsg(CryptoNote::RPC_DEFAULT_PORT) << ".\n\nHit enter for the default of localhost: ";

        std::string address;

        std::string host = "127.0.0.1";

        uint16_t port = CryptoNote::RPC_DEFAULT_PORT;

        bool ssl = false;

        /* Fixes infinite looping when someone does a ctrl + c */
        if (!std::getline(std::cin, address) || address == "")
        {
            return {host, port, ssl};
        }

        Utilities::trim(address);

        if (!Utilities::parseDaemonAddressFromString(host, port, address))
        {
            std::cout << WarningMsg("\nInvalid daemon address! Try again.\n");
            continue;
        }

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        ssl = Utilities::confirm("Does this daemon support SSL?", false);
#endif

        return {host, port, ssl};
    }
}

/* Template instantations that we are going to use - this allows us to have
   the template implementation in the .cpp file. */
template std::string getInput(const std::vector<Command> &availableCommands, std::string prompt);

template std::string getInput(const std::vector<AdvancedCommand> &availableCommands, std::string prompt);
