// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

/////////////////////////////////////
#include <zedwallet/ParseArguments.h>
/////////////////////////////////////

#include <config/CryptoNoteConfig.h>

#include "version.h"

#include <config/WalletConfig.h>

#include <zedwallet/Tools.h>

/* Thanks to https://stackoverflow.com/users/85381/iain for this small command
   line parsing snippet! https://stackoverflow.com/a/868894/8737306 */
char* getCmdOption(char ** begin, char ** end, const std::string & option)
{
    auto it = std::find(begin, end, option);

    if (it != end && ++it != end)
    {
        return *it;
    }

    return 0;
}

bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}

Config parseArguments(int argc, char **argv)
{
    Config config;

    if (cmdOptionExists(argv, argv+argc, "-h")
     || cmdOptionExists(argv, argv+argc, "--help"))
    {
        helpMessage();
        config.exit = true;
        return config;
    }

    if (cmdOptionExists(argv, argv+argc, "-v")
     || cmdOptionExists(argv, argv+argc, "--version"))
    {
        std::cout << getVersion() << std::endl;
        config.exit = true;
        return config;
    }

    const auto commands = getCLICommands();

    for (int i = 0; i < argc; i++)
    {
        /* If the argument is a command, for example --foo, check it exists */
        if (startsWith(argv[i], "--"))
        {
            const auto it = std::find_if(commands.begin(), commands.end(),
            [&argv, &i](const CLICommand command)
            {
                /* Only take the first word of the command, so instead of
                   --remote-daemon <url> we get the actual command,
                   --remote-daemon */
                std::string firstWord = command.name.substr(0, command.name.find(" "));
                return firstWord == argv[i];
            });

            if (it == commands.end())
            {
                std::cout << "Unknown command: " << argv[i] << std::endl
                          << std::endl;

                helpMessage();
                
                config.exit = true;
                return config;
            }
        }
    }

    if (cmdOptionExists(argv, argv+argc, "--debug"))
    {
        config.debug = true;
    }

    if (cmdOptionExists(argv, argv+argc, "--wallet-file"))
    {
        char *wallet = getCmdOption(argv, argv+argc, "--wallet-file");

        if (!wallet)
        {
            std::cout << "--wallet-file was specified, but no wallet file "
                      << "was given!" << std::endl;

            helpMessage();
            config.exit = true;
            return config;
        }

        config.walletFile = std::string(wallet);
        config.walletGiven = true;
    }

    if (cmdOptionExists(argv, argv+argc, "--password"))
    {
        char *password = getCmdOption(argv, argv+argc, "--password");

        if (!password)
        {
            std::cout << "--password was specified, but no password was "
                      << "given!" << std::endl;

            helpMessage();
            config.exit = true;
            return config;
        }

        config.walletPass = std::string(password);
        config.passGiven = true;
    }

    if (cmdOptionExists(argv, argv+argc, "--remote-daemon"))
    {
        char *url = getCmdOption(argv, argv + argc, "--remote-daemon");

        /* No url following --remote-daemon */
        if (!url)
        {
            std::cout << "--remote-daemon was specified, but no daemon was "
                      << "given!" << std::endl;

            helpMessage();

            config.exit = true;
        }
        else
        {
            std::string urlString(url);

            /* Get the index of the ":" */
            size_t splitter = urlString.find_last_of(":");

            /* Host is everything before ":" */
            config.host = urlString.substr(0, splitter);

            /* No ":" present, or user specifies http:// without port at end */
            if (splitter == std::string::npos || config.host == "http"
             || config.host == "https")
            {
                config.host = urlString;
            }
            else
            {
                /* Host is everything before ":" */
                config.host = urlString.substr(0, splitter);

                /* Port is everything after ":" */
                std::string port = urlString.substr(splitter + 1,   
                                                    std::string::npos);

                try
                {
                    config.port = std::stoi(port);
                }
                catch (const std::invalid_argument &)
                {
                    std::cout << "Failed to parse daemon port!" << std::endl;
                    config.exit = true;
                }
            }
        }
    }

    return config;
}

std::string getVersion()
{
    return WalletConfig::coinName + " v" + PROJECT_VERSION + " "
         + WalletConfig::walletName;
}

std::vector<CLICommand> getCLICommands()
{
    std::vector<CLICommand> commands =
    {
        {"--help", "Display this help message and exit", "-h", true, false},

        {"--version", "Display the version information and exit", "-v", true,
         false},

        {"--debug", "Enable " + WalletConfig::walletdName + " debugging to "
                  + WalletConfig::walletName + ".log", "", false, false},

        {"--remote-daemon <url>", "Connect to the remote daemon at <url>", "",
         false, true},

        {"--wallet-file <file>", "Open the wallet <file>", "", false, true},

        {"--password <pass>", "Use the password <pass> to open the wallet", "",
         false, true}
    };

    /* Pop em in alphabetical order */
    std::sort(commands.begin(), commands.end(), [](const CLICommand &lhs,
                                                   const CLICommand &rhs)
    {
        return lhs.name < rhs.name;
    });


    return commands;
}

void helpMessage()
{
    std::cout << getVersion() << std::endl;

    const auto commands = getCLICommands();

    std::cout << std::endl
              << WalletConfig::walletName;

    for (auto &command : commands)
    {
        if (command.hasArgument)
        {
            std::cout << " [" << command.name << "]";
        }
    }

    std::cout << std::endl << std::endl
              << "Commands: " << std::endl;

    for (auto &command : commands)
    {
        if (command.hasShortName)
        {
            std::cout << "  " << command.shortName << ", "
                      << std::left << std::setw(25) << command.name
                      << command.description << std::endl;
        }
        else
        {
            std::cout << "      " << std::left << std::setw(25) << command.name
                      << command.description << std::endl;
        }
    }
}
