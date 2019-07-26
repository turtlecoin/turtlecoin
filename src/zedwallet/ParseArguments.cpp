// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

/////////////////////////////////////
#include <zedwallet/ParseArguments.h>
/////////////////////////////////////

#include <cxxopts.hpp>
#include <config/CliHeader.h>
#include <config/CryptoNoteConfig.h>
#include <config/WalletConfig.h>
#include <zedwallet/Tools.h>
#include "version.h"

Config parseArguments(
    int argc,
    char **argv
)
{
    Config config;

    std::stringstream defaultRemoteDaemon;
    defaultRemoteDaemon << config.host << ":" << config.port;

    cxxopts::Options options(argv[0], CryptoNote::getProjectCLIHeader());

    bool help, version;
    std::string remoteDaemon;

    options.add_options("Core")(
        "h,help", "Display this help message", cxxopts::value<bool>(help)->implicit_value("true"))(
        "v,version", "Output software version information", cxxopts::value<bool>(
        version
    )->default_value("false")->implicit_value("true"))(
        "debug",
        "Enable " + WalletConfig::walletdName + " debugging to " + WalletConfig::walletName + ".log", cxxopts::value<
        bool
    >(config.debug)->default_value("false")->implicit_value("true"));

    options.add_options("Daemon")(
        "r,remote-daemon", "The daemon <host:port> combination to use for node operations.", cxxopts::value<
        std::string
    >(remoteDaemon)->default_value(defaultRemoteDaemon.str()), "<host:port>"
    );

    options.add_options("Wallet")(
        "w,wallet-file", "Open the wallet <file>", cxxopts::value<std::string>(config.walletFile), "<file>"
    )(
        "p,password", "Use the password <pass> to open the wallet", cxxopts::value<std::string>(
        config.walletPass
    ), "<pass>"
    );

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

    if (!config.walletFile.empty())
    {
        config.walletGiven = true;
    }

    if (!config.walletPass.empty())
    {
        config.passGiven = true;
    }

    if (!remoteDaemon.empty())
    {
        if (!parseDaemonAddressFromString(config.host, config.port, remoteDaemon))
        {
            std::cout << "There was an error parsing the --remote-daemon you specified" << std::endl;
            exit(1);
        }
    }

    return config;
}