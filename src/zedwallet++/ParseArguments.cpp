// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

///////////////////////////////////////
#include <zedwallet++/ParseArguments.h>
///////////////////////////////////////

#include <cxxopts.hpp>

#include <config/CliHeader.h>
#include <config/CryptoNoteConfig.h>
#include <config/WalletConfig.h>

#include <Utilities/Utilities.h>

#include "version.h"

#include <zedwallet++/Utilities.h>

Config parseArguments(int argc, char **argv)
{
    Config config;

    std::string defaultRemoteDaemon = "127.0.0.1:" + std::to_string(CryptoNote::RPC_DEFAULT_PORT);

    cxxopts::Options options(argv[0], CryptoNote::getProjectCLIHeader());

    bool help, version;

    std::string remoteDaemon;

    int logLevel;

    unsigned int threads;

    options.add_options("Core")
        ("h,help", "Display this help message", cxxopts::value<bool>(help)->implicit_value("true"))
        ("v,version", "Output software version information", cxxopts::value<bool>(version)->default_value("false")->implicit_value("true"));

    options.add_options("Daemon")
        ("r,remote-daemon", "The daemon <host:port> combination to use for node operations.",
          cxxopts::value<std::string>(remoteDaemon)->default_value(defaultRemoteDaemon), "<host:port>")
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        ("ssl", "Use SSL when connecting to the daemon.",
          cxxopts::value<bool>(config.ssl)->default_value("false")->implicit_value("true"))
#endif
        ;

    options.add_options("Wallet")
        ("w,wallet-file", "Open the wallet <file>", cxxopts::value<std::string>(config.walletFile), "<file>")
        ("p,password", "Use the password <pass> to open the wallet", cxxopts::value<std::string>(config.walletPass), "<pass>")
        ("log-level", "Specify log level", cxxopts::value<int>(logLevel)->default_value(std::to_string(config.logLevel)), "#")
        ("threads", "Specify number of wallet sync threads", cxxopts::value<unsigned int>(threads)->default_value(std::to_string(std::max(1u, std::thread::hardware_concurrency()))), "#");

    try
    {
        const auto result = options.parse(argc, argv);

        config.walletGiven = result.count("wallet-file") != 0;

        /* We could check if the string is empty, but an empty password is valid */
        config.passGiven = result.count("password") != 0;
    }
    catch (const cxxopts::OptionException& e)
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

    if (logLevel < Logger::DISABLED || logLevel > Logger::DEBUG)
    {
        std::cout << "Log level must be between " << Logger::DISABLED << " and " << Logger::DEBUG << "!" << std::endl;
        exit(1);
    }
    else
    {
        config.logLevel = static_cast<Logger::LogLevel>(logLevel);
    }

    if (threads == 0)
    {
        std::cout << "Thread count must be at least 1" << std::endl;
        exit(1);
    }
    else
    {
        config.threads = threads;
    }

    if (!remoteDaemon.empty())
    {
        if (!Utilities::parseDaemonAddressFromString(config.host, config.port, remoteDaemon))
        {
            std::cout << "There was an error parsing the --remote-daemon you specified" << std::endl;
            exit(1);
        }
    }

    return config;
}
