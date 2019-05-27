// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

/////////////////////////////////////
#include <WalletApi/ParseArguments.h>
/////////////////////////////////////

#include <cxxopts.hpp>

#include <config/CliHeader.h>
#include <config/CryptoNoteConfig.h>
#include <config/WalletConfig.h>

#include <thread>

#include "version.h"

Config parseArguments(int argc, char **argv)
{
    Config config;

    cxxopts::Options options(argv[0], CryptoNote::getProjectCLIHeader());

    bool help, version;

    int logLevel;

    unsigned int threads;

    options.add_options("Core")
        ("h,help", "Display this help message", cxxopts::value<bool>(help)->implicit_value("true"))
        ("log-level", "Specify log level", cxxopts::value<int>(logLevel)->default_value(std::to_string(config.logLevel)), "#")
        ("threads", "Specify number of wallet sync threads", cxxopts::value<unsigned int>(threads)->default_value(std::to_string(std::max(1u, std::thread::hardware_concurrency()))), "#")
        ("v,version", "Output software version information", cxxopts::value<bool>(version)->default_value("false")->implicit_value("true"));

    options.add_options("Network")
        ("p,port", "The port to listen on for http requests",
        cxxopts::value<uint16_t>(config.port)->default_value(std::to_string(CryptoNote::SERVICE_DEFAULT_PORT)), "<port>")

        ("rpc-bind-ip", "Interface IP address for the RPC service",
        cxxopts::value<std::string>(config.rpcBindIp)->default_value("127.0.0.1"));

    options.add_options("RPC")
        ("enable-cors", "Adds header 'Access-Control-Allow-Origin' to the RPC responses. Uses the value specified as the domain. Use * for all.",
        cxxopts::value<std::string>(config.corsHeader), "<domain>")

        ("r,rpc-password", "Specify the <password> to access the RPC server.", cxxopts::value<std::string>(config.rpcPassword), "<password>");

    try
    {
        const auto result = options.parse(argc, argv);

        /* Rpc password must be specified if not --help or --version */
        if (result.count("rpc-password") == 0 && !(help || version))
        {
            std::cout << "You must specify an rpc-password!" << std::endl;
            std::cout << options.help({}) << std::endl;
            exit(1);
        }
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

    return config;
}
