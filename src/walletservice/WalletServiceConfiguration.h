// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <config/CryptoNoteConfig.h>
#include <logging/ILogger.h>
#include <rapidjson/document.h>
#include <string>

using namespace rapidjson;

namespace PaymentService
{
    struct WalletServiceConfiguration
    {
        WalletServiceConfiguration() {};

        /* Address for the daemon RPC */
        std::string daemonAddress = "127.0.0.1";

        /* Address to run the API on (0.0.0.0 for all interfaces) */
        std::string bindAddress = "127.0.0.1";

        /* Password to access the API */
        std::string rpcPassword;

        /* Location of wallet file on disk */
        std::string containerFile;

        /* Password for the wallet file */
        std::string containerPassword;

        std::string serverRoot;

        /* Value to set for Access-Control-Allow-Origin (* for all) */
        std::string corsHeader;

        /* File to log to */
        std::string logFile = "service.log";

        /* Port to use for the daemon RPC */
        int daemonPort = CryptoNote::RPC_DEFAULT_PORT;

        /* Port for the API to listen on */
        int bindPort = CryptoNote::SERVICE_DEFAULT_PORT;

        /* Default log level */
        int logLevel = Logging::INFO;

        /* Timeout for daemon connection in seconds */
        int initTimeout = 10;

        /* Should we disable RPC connection */
        bool legacySecurity = false;

        /* Should we display the help message */
        bool help = false;

        /* Should we display the version message */
        bool version = false;

        /* Should we dump the provided config */
        bool dumpConfig = false;

        /* File to load config from */
        std::string configFile;

        /* File to dump the provided config to */
        std::string outputFile;

        /* Private view key to import */
        std::string secretViewKey;

        /* Private spend key to import */
        std::string secretSpendKey;

        /* Mnemonic seed to import */
        std::string mnemonicSeed;

        /* Should we create a new wallet */
        bool generateNewContainer = false;

        bool daemonize = false;

        bool registerService = false;

        bool unregisterService = false;

        /* Print all the addresses and exit (Why is this a thing?) */
        bool printAddresses = false;

        bool syncFromZero = false;

        /* Height to begin scanning at (on initial import) */
        uint64_t scanHeight;
    };

    bool updateConfigFormat(const std::string configFile, WalletServiceConfiguration &config);

    void handleSettings(int argc, char *argv[], WalletServiceConfiguration &config);

    void handleSettings(const std::string configFile, WalletServiceConfiguration &config);

    Document asJSON(const WalletServiceConfiguration &config);

    std::string asString(const WalletServiceConfiguration &config);

    void asFile(const WalletServiceConfiguration &config, const std::string &filename);
} // namespace PaymentService
