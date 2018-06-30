// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "Common/SignalHandler.h"

#include "Logging/LoggerGroup.h"
#include "Logging/ConsoleLogger.h"
#include "Logging/LoggerRef.h"

#include "MinerManager.h"

#include <System/Dispatcher.h>

int main(int argc, char **argv)
{
    try
    {
        CryptoNote::MiningConfig config;
        config.parse(argc, argv);

        if (config.help)
        {
            config.printHelp();
            return 0;
        }

        if (config.version)
        {
            config.printVersion();
            return 0;
        }

        Logging::LoggerGroup loggerGroup;
        Logging::ConsoleLogger consoleLogger(static_cast<Logging::Level>(config.logLevel));
        loggerGroup.addLogger(consoleLogger);

        System::Dispatcher dispatcher;
        Miner::MinerManager app(dispatcher, config, loggerGroup);

        app.start();
    } catch (std::exception &e)
    {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
