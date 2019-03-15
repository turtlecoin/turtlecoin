// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

////////////////////////////////
#include <zedwallet/ZedWallet.h>
////////////////////////////////

#include <config/CliHeader.h>
#include <Common/SignalHandler.h>
#include <CryptoNoteCore/Currency.h>
#include <Logging/FileLogger.h>
#include <Logging/LoggerManager.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <Utilities/ColouredMsg.h>
#include <zedwallet/Menu.h>
#include <zedwallet/ParseArguments.h>
#include <zedwallet/Tools.h>
#include <config/WalletConfig.h>

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

    std::cout << InformationMsg(CryptoNote::getProjectCLIHeader()) << std::endl;

    const auto logManager = std::make_shared<Logging::LoggerManager>();

    if (config.debug)
    {
        logManager->setMaxLevel(Logging::DEBUGGING);

        Logging::FileLogger fileLogger;

        fileLogger.init(WalletConfig::walletName + ".log");
        logManager->addLogger(fileLogger);
    }

    /* Currency contains our coin parameters, such as decimal places, supply */
    const CryptoNote::Currency currency 
        = CryptoNote::CurrencyBuilder(logManager).currency();

    System::Dispatcher localDispatcher;
    System::Dispatcher *dispatcher = &localDispatcher;

    /* Our connection to turtlecoind */
    std::unique_ptr<CryptoNote::INode> node(
        new CryptoNote::NodeRpcProxy(config.host, config.port, 10, logManager)
    );

    std::promise<std::error_code> errorPromise;

    /* Once the function is complete, set the error value from the promise */
    auto callback = [&errorPromise](std::error_code e)
    {
        errorPromise.set_value(e);
    };

    /* Get the future of the result */
    auto initNode = errorPromise.get_future();

    node->init(callback);

    /* Connection took too long to remote node, let program continue regardless
       as they could perform functions like export_keys without being
       connected */
    if (initNode.wait_for(std::chrono::seconds(20)) != std::future_status::ready)
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
    CryptoNote::WalletGreen wallet(*dispatcher, currency, *node, logManager);

    /* Run the interactive wallet interface */
    run(wallet, *node, config);
}

void run(CryptoNote::WalletGreen &wallet, CryptoNote::INode &node,
         Config &config)
{
    auto [quit, walletInfo] = selectionScreen(config, wallet, node);

    bool alreadyShuttingDown = false;

    if (!quit)
    {
        /* Call shutdown on ctrl+c */
        /* walletInfo = walletInfo - workaround for
           https://stackoverflow.com/a/46115028/8737306 - standard &
           capture works in newer compilers. */
        Tools::SignalHandler::install([&walletInfo = walletInfo, &node,
                                       &alreadyShuttingDown]
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
