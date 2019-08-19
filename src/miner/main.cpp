// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "MinerManager.h"

#include <system/Dispatcher.h>

int main(int argc, char **argv)
{
    while (true)
    {
        CryptoNote::MiningConfig config;
        config.parse(argc, argv);

        try
        {
            System::Dispatcher dispatcher;

            auto httpClient = std::make_shared<httplib::Client>(
                config.daemonHost.c_str(), config.daemonPort, 10 /* 10 second timeout */
            );

            Miner::MinerManager app(dispatcher, config, httpClient);

            app.start();
        }
        catch (const std::exception &e)
        {
            std::cout << "Unhandled exception caught: " << e.what() << "\nAttempting to relaunch..." << std::endl;
        }
    }
}
