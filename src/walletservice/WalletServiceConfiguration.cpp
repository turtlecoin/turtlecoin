// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "WalletServiceConfiguration.h"

#include "rapidjson/stringbuffer.h"

#include <config/CliHeader.h>
#include <config/CryptoNoteConfig.h>
#include <cxxopts.hpp>
#include <fstream>
#include <logging/ILogger.h>
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/writer.h>
#include <string>

using namespace rapidjson;

namespace PaymentService
{
    void handleSettings(int argc, char *argv[], WalletServiceConfiguration &config)
    {
        cxxopts::Options options(argv[0], CryptoNote::getProjectCLIHeader());

        options.add_options("Core")(
            "h,help", "Display this help message", cxxopts::value<bool>()->implicit_value("true"))(
            "v,version",
            "Output software version information",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"));

        options.add_options("Daemon")(
            "daemon-address",
            "The daemon host to use for node operations",
            cxxopts::value<std::string>()->default_value(config.daemonAddress),
            "<ip>")(
            "daemon-port",
            "The daemon RPC port to use for node operations",
            cxxopts::value<int>()->default_value(std::to_string(config.daemonPort)),
            "<port>");

        options.add_options("Service")(
            "c,config",
            "Specify the configuration <file> to use instead of CLI arguments",
            cxxopts::value<std::string>(),
            "<file>")(
            "dump-config",
            "Prints the current configuration to the screen",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "l,log-file",
            "Specify log <file> location",
            cxxopts::value<std::string>()->default_value(config.logFile),
            "<file>")(
            "log-level",
            "Specify log level",
            cxxopts::value<int>()->default_value(std::to_string(config.logLevel)),
            "#")(
            "server-root",
            "The service will use this <path> as the working directory",
            cxxopts::value<std::string>(),
            "<path>")(
            "save-config", "Save the configuration to the specified <file>", cxxopts::value<std::string>(), "<file>")(
            "init-timeout",
            "Amount of time in seconds to wait for initial connection",
            cxxopts::value<int>()->default_value("10"),
            "<seconds>");

        options.add_options("Wallet")(
            "address",
            "Print the wallet addresses and then exit",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "w,container-file", "Wallet container <file>", cxxopts::value<std::string>(), "<file>")(
            "p,container-password", "Wallet container <password>", cxxopts::value<std::string>(), "<password>")(
            "g,generate-container",
            "Generate a new wallet container",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "view-key",
            "Generate a wallet container with this secret view <key>",
            cxxopts::value<std::string>(),
            "<key>")(
            "spend-key",
            "Generate a wallet container with this secret spend <key>",
            cxxopts::value<std::string>(),
            "<key>")(
            "mnemonic-seed",
            "Generate a wallet container with this Mnemonic <seed>",
            cxxopts::value<std::string>(),
            "<seed>")(
            "scan-height",
            "Start scanning for transactions from this Blockchain height",
            cxxopts::value<uint64_t>()->default_value("0"),
            "#")(
            "SYNC_FROM_ZERO",
            "Force the wallet to sync from 0",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"));

        options.add_options("Network")(
            "bind-address",
            "Interface IP address for the RPC service",
            cxxopts::value<std::string>()->default_value(config.bindAddress),
            "<ip>")(
            "bind-port",
            "TCP port for the RPC service",
            cxxopts::value<int>()->default_value(std::to_string(config.bindPort)),
            "<port>");

        options.add_options("RPC")(
            "enable-cors",
            "Adds header 'Access-Control-Allow-Origin' to the RPC responses. Uses the value specified as the domain. "
            "Use * for all.",
            cxxopts::value<std::string>(),
            "<domain>")(
            "rpc-legacy-security",
            "Enable legacy mode (no password for RPC). WARNING: INSECURE. USE ONLY AS A LAST RESORT.",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "rpc-password",
            "Specify the <password> to access the RPC server.",
            cxxopts::value<std::string>(),
            "<password>");

#ifdef WIN32
        options.add_options("Windows Only")(
            "daemonize",
            "Run the service as a daemon",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "register-service",
            "Registers this program as a Windows service",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"))(
            "unregister-service",
            "Unregisters this program from being a Windows service",
            cxxopts::value<bool>()->default_value("false")->implicit_value("true"));
#endif

        try
        {
            auto cli = options.parse(argc, argv);

            if (cli.count("help") > 0)
            {
                config.help = cli["help"].as<bool>();
            }

            if (cli.count("version") > 0)
            {
                config.version = cli["version"].as<bool>();
            }

            if (cli.count("config") > 0)
            {
                config.configFile = cli["config"].as<std::string>();
            }

            if (cli.count("save-config") > 0)
            {
                config.outputFile = cli["save-config"].as<std::string>();
            }

            if (cli.count("dump-config") > 0)
            {
                config.dumpConfig = cli["dump-config"].as<bool>();
            }

            if (cli.count("daemon-address") > 0)
            {
                config.daemonAddress = cli["daemon-address"].as<std::string>();
            }

            if (cli.count("daemon-port") > 0)
            {
                config.daemonPort = cli["daemon-port"].as<int>();
            }

            if (cli.count("init-timeout") > 0)
            {
                config.initTimeout = cli["init-timeout"].as<int>();
            }

            if (cli.count("log-file") > 0)
            {
                config.logFile = cli["log-file"].as<std::string>();
            }

            if (cli.count("log-level") > 0)
            {
                config.logLevel = cli["log-level"].as<int>();
            }

            if (cli.count("container-file") > 0)
            {
                config.containerFile = cli["container-file"].as<std::string>();
            }

            if (cli.count("container-password") > 0)
            {
                config.containerPassword = cli["container-password"].as<std::string>();
            }

            if (cli.count("bind-address") > 0)
            {
                config.bindAddress = cli["bind-address"].as<std::string>();
            }

            if (cli.count("bind-port") > 0)
            {
                config.bindPort = cli["bind-port"].as<int>();
            }

            if (cli.count("enable-cors") > 0)
            {
                config.corsHeader = cli["enable-cors"].as<std::string>();
            }

            if (cli.count("rpc-legacy-security") > 0)
            {
                config.legacySecurity = cli["rpc-legacy-security"].as<bool>();
            }

            if (cli.count("rpc-password") > 0)
            {
                config.rpcPassword = cli["rpc-password"].as<std::string>();
            }

            if (cli.count("server-root") > 0)
            {
                config.serverRoot = cli["server-root"].as<std::string>();
            }

            if (cli.count("view-key") > 0)
            {
                config.secretViewKey = cli["view-key"].as<std::string>();
            }

            if (cli.count("spend-key") > 0)
            {
                config.secretSpendKey = cli["spend-key"].as<std::string>();
            }

            if (cli.count("mnemonic-seed") > 0)
            {
                config.mnemonicSeed = cli["mnemonic-seed"].as<std::string>();
            }

            if (cli.count("generate-container") > 0)
            {
                config.generateNewContainer = cli["generate-container"].as<bool>();
            }

            if (cli.count("daemonize") > 0)
            {
                config.daemonize = cli["daemonize"].as<bool>();
            }

            if (cli.count("register-service") > 0)
            {
                config.registerService = cli["register-service"].as<bool>();
            }

            if (cli.count("unregister-service") > 0)
            {
                config.unregisterService = cli["unregister-service"].as<bool>();
            }

            if (cli.count("address") > 0)
            {
                config.printAddresses = cli["address"].as<bool>();
            }

            if (cli.count("SYNC_FROM_ZERO") > 0)
            {
                config.syncFromZero = cli["SYNC_FROM_ZERO"].as<bool>();
            }

            if (cli.count("scan-height") > 0)
            {
                config.scanHeight = cli["scan-height"].as<uint64_t>();
            }

            if (config.help) // Do we want to display the help message?
            {
                std::cout << options.help({}) << std::endl;
                exit(0);
            }
            else if (config.version) // Do we want to display the software version?
            {
                std::cout << CryptoNote::getProjectCLIHeader() << std::endl;
                exit(0);
            }
        }
        catch (const cxxopts::OptionException &e)
        {
            std::cout << "Error: Unable to parse command line argument options: " << e.what() << std::endl
                      << std::endl
                      << options.help({}) << std::endl;
            exit(1);
        }
    }

    bool updateConfigFormat(const std::string configFile, WalletServiceConfiguration &config)
    {
        std::ifstream data(configFile);

        if (!data.good())
        {
            throw std::runtime_error(
                "The --config-file you specified does not exist, please check the filename and try again.");
        }
        // find key=value pair, respect whitespace before/after "="
        // g0: full match, g1: match key, g2: match value
        static const std::regex cfgItem {R"x(\s*(\S[^ \t=]*)\s*=\s*((\s?\S+)+)\s*$)x"};

        // comments, first non space starts with # or ;
        static const std::regex cfgComment {R"x(\s*[;#])x"};
        std::smatch item;
        std::string cfgKey;
        std::string cfgValue;
        bool updated = false;

        for (std::string line; std::getline(data, line);)
        {
            if (line.empty() || std::regex_match(line, item, cfgComment))
            {
                continue;
            }

            if (std::regex_match(line, item, cfgItem))
            {
                if (item.size() != 4)
                {
                    continue;
                }

                cfgKey = item[1].str();
                cfgValue = item[2].str();

                if (cfgKey.compare("daemon-address") == 0)
                {
                    config.daemonAddress = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("daemon-port") == 0)
                {
                    try
                    {
                        config.daemonPort = std::stoi(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("init-timeout") == 0)
                {
                    try
                    {
                        config.initTimeout = std::stoi(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("log-file") == 0)
                {
                    config.logFile = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("log-level") == 0)
                {
                    try
                    {
                        config.logLevel = std::stoi(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("container-file") == 0)
                {
                    config.containerFile = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("container-password") == 0)
                {
                    config.containerPassword = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("bind-address") == 0)
                {
                    config.bindAddress = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("bind-port") == 0)
                {
                    try
                    {
                        config.bindPort = std::stoi(cfgValue);
                        updated = true;
                    }
                    catch (std::exception &e)
                    {
                        throw std::runtime_error(std::string(e.what()) + " - Invalid value for " + cfgKey);
                    }
                }
                else if (cfgKey.compare("enable-cors") == 0)
                {
                    config.corsHeader = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("rpc-legacy-security") == 0)
                {
                    config.legacySecurity = cfgValue.at(0) == '1' ? true : false;
                    updated = true;
                }
                else if (cfgKey.compare("rpc-password") == 0)
                {
                    config.rpcPassword = cfgValue;
                    updated = true;
                }
                else if (cfgKey.compare("server-root") == 0)
                {
                    config.serverRoot = cfgValue;
                    updated = true;
                }
                else
                {
                    for (auto c : cfgKey)
                    {
                        if (static_cast<unsigned char>(c) > 127)
                        {
                            throw std::runtime_error(std::string("Bad/invalid config file"));
                        }
                    }
                    throw std::runtime_error("Unknown option: " + cfgKey);
                }
            }
        }

        if (!updated)
        {
            return false;
        }

        try
        {
            std::ifstream orig(configFile, std::ios::binary);
            std::ofstream backup(configFile + ".ini.bak", std::ios::binary);
            backup << orig.rdbuf();
        }
        catch (std::exception &e)
        {
            // pass
        }
        return updated;
    }

    void handleSettings(const std::string configFile, WalletServiceConfiguration &config)
    {
        std::ifstream data(configFile);

        if (!data.good())
        {
            throw std::runtime_error(
                "The --config-file you specified does not exist, please check the filename and try again.");
        }

        IStreamWrapper isw(data);
        Document j;
        j.ParseStream(isw);

        if (j.HasMember("daemon-address"))
        {
            config.daemonAddress = j["daemon-address"].GetString();
        }

        if (j.HasMember("daemon-port"))
        {
            config.daemonPort = j["daemon-port"].GetInt();
        }

        if (j.HasMember("init-timeout"))
        {
            config.initTimeout = j["init-timeout"].GetInt();
        }

        if (j.HasMember("log-file"))
        {
            config.logFile = j["log-file"].GetString();
        }

        if (j.HasMember("log-level"))
        {
            config.logLevel = j["log-level"].GetInt();
        }

        if (j.HasMember("container-file"))
        {
            config.containerFile = j["container-file"].GetString();
        }

        if (j.HasMember("container-password"))
        {
            config.containerPassword = j["container-password"].GetString();
        }

        if (j.HasMember("bind-address"))
        {
            config.bindAddress = j["bind-address"].GetString();
        }

        if (j.HasMember("bind-port"))
        {
            config.bindPort = j["bind-port"].GetInt();
        }

        if (j.HasMember("enable-cors"))
        {
            config.corsHeader = j["enable-cors"].GetString();
        }

        if (j.HasMember("rpc-legacy-security"))
        {
            config.legacySecurity = j["rpc-legacy-security"].GetBool();
        }

        if (j.HasMember("rpc-password"))
        {
            config.rpcPassword = j["rpc-password"].GetString();
        }

        if (j.HasMember("server-root"))
        {
            config.serverRoot = j["server-root"].GetString();
        }
    }

    Document asJSON(const WalletServiceConfiguration &config)
    {
        Document j;
        j.SetObject();
        Document::AllocatorType &alloc = j.GetAllocator();

        j.AddMember("daemon-address", config.daemonAddress, alloc);
        j.AddMember("daemon-port", config.daemonPort, alloc);
        j.AddMember("log-file", config.logFile, alloc);
        j.AddMember("log-level", config.logLevel, alloc);
        j.AddMember("init-timeout", config.initTimeout, alloc);
        j.AddMember("container-file", config.containerFile, alloc);
        j.AddMember("container-password", config.containerPassword, alloc);
        j.AddMember("bind-address", config.bindAddress, alloc);
        j.AddMember("bind-port", config.bindPort, alloc);
        j.AddMember("enable-cors", config.corsHeader, alloc);
        j.AddMember("rpc-legacy-security", config.legacySecurity, alloc);
        j.AddMember("rpc-password", config.rpcPassword, alloc);
        j.AddMember("server-root", config.serverRoot, alloc);

        return j;
    }

    std::string asString(const WalletServiceConfiguration &config)
    {
        StringBuffer strbuf;
        PrettyWriter<StringBuffer> writer(strbuf);
        Document j = asJSON(config);
        j.Accept(writer);
        return strbuf.GetString();
    }

    void asFile(const WalletServiceConfiguration &config, const std::string &filename)
    {
        Document j = asJSON(config);
        std::ofstream data(filename);
        OStreamWrapper osw(data);
        PrettyWriter<OStreamWrapper> writer(osw);
        j.Accept(writer);
    }
} // namespace PaymentService
