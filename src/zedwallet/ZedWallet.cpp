// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

////////////////////////////////
#include <zedwallet/ZedWallet.h>
////////////////////////////////

#include <Common/SignalHandler.h>

#include <CryptoNoteCore/Currency.h>

#include <Logging/FileLogger.h>
#include <Logging/LoggerManager.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <zedwallet/ColouredMsg.h>
#include <zedwallet/Menu.h>
#include <zedwallet/ParseArguments.h>
#include <zedwallet/Tools.h>
#include <zedwallet/WalletConfig.h>

int main(int argc, char **argv)
{
    /* On ctrl+c the program seems to throw "zedwallet.exe has stopped
       working" when calling exit(0)... I'm not sure why, this is a bit of
       a hack, it disables that - possibly some deconstructers calling
       terminate() */
    #ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    #endif

    Config config = parseArguments(argc, argv);

    /* User requested --help or --version, or invalid arguments */
    if (config.exit)
    {
        return 0;
    }

    Logging::LoggerManager logManager;

    /* We'd like these lines to be in the below if(), but because some genius
       thought it was a good idea to pass everything by reference and then
       use them after the functions lifetime they go out of scope and break
       stuff */
    logManager.setMaxLevel(Logging::DEBUGGING);

    Logging::FileLogger fileLogger;

    if (config.debug)
    {
        fileLogger.init(WalletConfig::walletName + ".log");
        logManager.addLogger(fileLogger);
    }

    Logging::LoggerRef logger(logManager, WalletConfig::walletName);

    /* Currency contains our coin parameters, such as decimal places, supply */
    const CryptoNote::Currency currency 
        = CryptoNote::CurrencyBuilder(logManager).currency();

    System::Dispatcher localDispatcher;
    System::Dispatcher *dispatcher = &localDispatcher;

    /* Our connection to turtlecoind */
    std::unique_ptr<CryptoNote::INode> node(
        new CryptoNote::NodeRpcProxy(config.host, config.port, 
                                     logger.getLogger()));

    std::promise<std::error_code> errorPromise;
    std::future<std::error_code> error = errorPromise.get_future();
    auto callback = [&errorPromise](std::error_code e) 
                    {errorPromise.set_value(e); };

    node->init(callback);

    std::future<void> initNode = std::async(std::launch::async, [&]
    {
            if (error.get())
            {
                throw std::runtime_error("Failed to initialize node!");
            }
    });

    std::future_status status = initNode.wait_for(std::chrono::seconds(20));

    /* Connection took to long to remote node, let program continue regardless
       as they could perform functions like export_keys without being
       connected */
    if (status != std::future_status::ready)
    {
        if (config.host != "127.0.0.1")
        {
            std::cout << WarningMsg("Unable to connect to remote node, "
                                    "connection timed out.")
                      << std::endl
                      << WarningMsg("Confirm the remote node is functioning, "
                                    "or try a different remote node.")
                      << std::endl << std::endl;
        }
        else
        {
            std::cout << WarningMsg("Unable to connect to node, "
                                    "connection timed out.")
                      << std::endl << std::endl;
        }
    }
    
    /*
      This will check to see if the node responded to /feeinfo and actually
      returned something that it expects us to use for convenience charges
      for using that node to send transactions.
    */
    if (node->feeAmount() != 0 && !node->feeAddress().empty()) {
      std::stringstream feemsg;
      
      feemsg << std::endl << "You have connected to a node that charges " <<
             "a fee to send transactions." << std::endl << std::endl
             << "The fee for sending transactions is: " << 
             formatAmount(node->feeAmount()) << 
             " per transaction." << std::endl << std::endl <<
             "If you don't want to pay the node fee, please " <<
             "relaunch " << WalletConfig::walletName <<
             " and specify a different node or run your own." <<
             std::endl;
             
      std::cout << WarningMsg(feemsg.str()) << std::endl;
    }

    /* Create the wallet instance */
    CryptoNote::WalletGreen wallet(*dispatcher, currency, *node, 
                                   logger.getLogger());

    /* Run the interactive wallet interface */
    run(wallet, *node, config);
}

void run(CryptoNote::WalletGreen &wallet, CryptoNote::INode &node,
         Config &config)
{
    std::cout << InformationMsg(getVersion()) << std::endl;

    std::shared_ptr<WalletInfo> walletInfo;

    bool quit;

    std::tie(quit, walletInfo) = selectionScreen(config, wallet, node);

    bool alreadyShuttingDown = false;

    if (!quit)
    {
        /* Call shutdown on ctrl+c */
        Tools::SignalHandler::install([&]
        {
            /* If we're already shutting down let control flow continue
               as normal */
            if (shutdown(walletInfo, node, alreadyShuttingDown))
            {
                exit(0);
            }
        });

        mainLoop(walletInfo, node);
    }
    
    shutdown(walletInfo, node, alreadyShuttingDown);
}

bool shutdown(std::shared_ptr<WalletInfo> walletInfo, CryptoNote::INode &node,
              bool &alreadyShuttingDown)
{
    if (alreadyShuttingDown)
    {
        std::cout << "Patience little turtle, we're already shutting down!" 
                  << std::endl;

        return false;
    }

    std::cout << InformationMsg("Shutting down...") << std::endl;

    alreadyShuttingDown = true;

    bool finishedShutdown = false;

    std::thread timelyShutdown([&finishedShutdown]
    {
        const auto startTime = std::chrono::system_clock::now();

        /* Has shutdown finished? */
        while (!finishedShutdown)
        {
            const auto currentTime = std::chrono::system_clock::now();

            /* If not, wait for a max of 20 seconds then force exit. */
            if ((currentTime - startTime) > std::chrono::seconds(20))
            {
                std::cout << WarningMsg("Wallet took too long to save! "
                                        "Force closing.") << std::endl
                          << "Bye." << std::endl;
                exit(0);
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    if (walletInfo != nullptr)
    {
        std::cout << InformationMsg("Saving wallet file...") << std::endl;

        walletInfo->wallet.save();

        std::cout << InformationMsg("Shutting down wallet interface...")
                  << std::endl;

        walletInfo->wallet.shutdown();
    }

    std::cout << InformationMsg("Shutting down node connection...")
              << std::endl;

    node.shutdown();

    finishedShutdown = true;

    /* Wait for shutdown watcher to finish */
    timelyShutdown.join();

    std::cout << "Bye." << std::endl;
    
    return true;
}
