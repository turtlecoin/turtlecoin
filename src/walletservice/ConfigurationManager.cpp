// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "ConfigurationManager.h"

#include "common/Util.h"
#include "crypto/hash.h"
#include "logging/ILogger.h"

#include <CryptoTypes.h>
#include <config/CliHeader.h>
#include <config/CryptoNoteConfig.h>
#include <fstream>
#include <iostream>

namespace PaymentService
{
    ConfigurationManager::ConfigurationManager()
    {
        rpcSecret = Crypto::Hash();
    }

    bool ConfigurationManager::init(int argc, char **argv)
    {
        // Load in the initial CLI options
        handleSettings(argc, argv, serviceConfig);

        // If the user passed in the --config-file option, we need to handle that first
        if (!serviceConfig.configFile.empty())
        {
            // check if it's old config format & try converting into new format
            try
            {
                if (updateConfigFormat(serviceConfig.configFile, serviceConfig))
                {
                    std::cout << std::endl << "Updating configuration file format..." << std::endl;
                    asFile(serviceConfig, serviceConfig.configFile);
                }
            }
            catch (std::runtime_error &e)
            {
                std::cout << std::endl
                          << "There was an error parsing the specified configuration file. Please check the file and "
                             "try again:"
                          << std::endl
                          << e.what() << std::endl;
                exit(1);
            }
            catch (std::exception &e)
            {
                // pass
            }

            try
            {
                handleSettings(serviceConfig.configFile, serviceConfig);
            }
            catch (std::exception &e)
            {
                std::cout << std::endl
                          << "There was an error parsing the specified configuration file. Please check the file and "
                             "try again: "
                          << std::endl
                          << e.what() << std::endl;
                exit(1);
            }
        }

        // Load in the CLI specified parameters again to overwrite any given in the config file
        handleSettings(argc, argv, serviceConfig);

        if (serviceConfig.dumpConfig)
        {
            std::cout << CryptoNote::getProjectCLIHeader() << asString(serviceConfig) << std::endl;
            exit(0);
        }
        else if (!serviceConfig.outputFile.empty())
        {
            try
            {
                asFile(serviceConfig, serviceConfig.outputFile);
                std::cout << CryptoNote::getProjectCLIHeader() << "Configuration saved to: " << serviceConfig.outputFile
                          << std::endl;
                exit(0);
            }
            catch (std::exception &e)
            {
                std::cout << CryptoNote::getProjectCLIHeader()
                          << "Could not save configuration to: " << serviceConfig.outputFile << std::endl
                          << e.what() << std::endl;
                exit(1);
            }
        }

        if (serviceConfig.registerService && serviceConfig.unregisterService)
        {
            throw std::runtime_error(
                "It's impossible to use both --register-service and --unregister-service at the same time");
        }

        if (serviceConfig.logLevel > Logging::TRACE)
        {
            throw std::runtime_error(
                "log-level must be between " + std::to_string(Logging::FATAL) + ".." + std::to_string(Logging::TRACE));
        }

        if (serviceConfig.containerFile.empty())
        {
            throw std::runtime_error("You must specify a wallet file to open!");
        }

        if (!std::ifstream(serviceConfig.containerFile) && !serviceConfig.generateNewContainer)
        {
            if (std::ifstream(serviceConfig.containerFile + ".wallet"))
            {
                throw std::runtime_error(
                    "The wallet file you specified does not exist. Did you mean: " + serviceConfig.containerFile
                    + ".wallet?");
            }
            else
            {
                throw std::runtime_error(
                    "The wallet file you specified does not exist; please check your spelling and try again.");
            }
        }

        if ((!serviceConfig.secretViewKey.empty() || !serviceConfig.secretSpendKey.empty())
            && !serviceConfig.generateNewContainer)
        {
            throw std::runtime_error("--generate-container is required");
        }

        if (!serviceConfig.mnemonicSeed.empty() && !serviceConfig.generateNewContainer)
        {
            throw std::runtime_error("--generate-container is required");
        }

        if (!serviceConfig.mnemonicSeed.empty()
            && (!serviceConfig.secretViewKey.empty() || !serviceConfig.secretSpendKey.empty()))
        {
            throw std::runtime_error("You cannot specify import from both Mnemonic seed and private keys");
        }

        if ((serviceConfig.registerService || serviceConfig.unregisterService) && serviceConfig.containerFile.empty())
        {
            throw std::runtime_error("--container-file parameter is required");
        }

        if (serviceConfig.generateNewContainer)
        {
            return true;
        }

        // Run authentication checks

        if (serviceConfig.rpcPassword.empty() && !serviceConfig.legacySecurity)
        {
            throw std::runtime_error("Please specify either an RPC password or use the --rpc-legacy-security flag");
        }

        if (!serviceConfig.rpcPassword.empty())
        {
            std::vector<uint8_t> rawData(serviceConfig.rpcPassword.begin(), serviceConfig.rpcPassword.end());
            Crypto::cn_slow_hash_v0(rawData.data(), rawData.size(), rpcSecret);
            serviceConfig.rpcPassword = "";
        }

        return true;
    }

} // namespace PaymentService
