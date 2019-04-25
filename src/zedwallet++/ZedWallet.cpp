// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#include <iostream>

#include <Common/SignalHandler.h>

#include <config/CliHeader.h>

#include <Utilities/ColouredMsg.h>
#include <zedwallet++/Menu.h>
#include <zedwallet++/ParseArguments.h>
#include <zedwallet++/Sync.h>
#include <zedwallet++/TransactionMonitor.h>

void shutdown(
    const std::atomic<bool> &ctrl_c,
    const std::atomic<bool> &stop,
    const std::shared_ptr<WalletBackend> walletBackend)
{
    while (!ctrl_c && !stop)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    /* Return if we're exiting normally, not via ctrl-c
       We could just detach this thread, but then walletBackend will never
       have the destructor called, since it is still running in this thread */
    if (stop)
    {
        return;
    }

    if (walletBackend != nullptr)
    {
        std::cout << InformationMsg("\nSaving and shutting down...\n");

        /* Delete the walletbackend - this will call the deconstructor,
           which will set the appropriate m_shouldStop flag. Since this
           function gets triggered from a signal handler, we can't just call
           save() - The data may be in an invalid state. 
           
           Obviously, calling delete on a shared pointer is undefined
           behaviour if we continue using it in another thread - fortunately,
           we're exiting right now. */
        walletBackend->~WalletBackend();
    }

    exit(0);
}

void cleanup(
    std::thread &txMonitorThread,
    std::thread &ctrlCWatcher,
    std::atomic<bool> &stop,
    std::shared_ptr<TransactionMonitor> txMonitor)
{
    /* Stop the transaction monitor */
    txMonitor->stop();

    /* Signal the ctrlCWatcher to stop */
    stop = true;

    /* Wait for the transaction monitor to stop */
    if (txMonitorThread.joinable())
    {
        txMonitorThread.join();
    }

    /* Wait for the ctrlCWatcher to stop */
    if (ctrlCWatcher.joinable())
    {
        ctrlCWatcher.join();
    }
}

int main(int argc, char **argv)
{
    Config config = parseArguments(argc, argv);

    Logger::logger.setLogLevel(config.logLevel);

    std::cout << InformationMsg(CryptoNote::getProjectCLIHeader()) << std::endl;

    /* Declare outside the try/catch, so if an exception is thrown, it doesn't
       cause the threads to go out of scope, calling std::terminate
       (since we didn't join them) */
    std::thread ctrlCWatcher, txMonitorThread;

    std::shared_ptr<TransactionMonitor> txMonitor(nullptr);

    /* Atomic bool to signal if ctrl_c is used */
    std::atomic<bool> ctrl_c(false), stop(false);

    try
    {
        const auto [quit, sync, walletBackend] = selectionScreen(config);

        if (quit)
        {
            std::cout << "Thanks for stopping by..." << std::endl;
            return 0;
        }

        /* Launch the thread which watches for the shutdown signal */
        ctrlCWatcher = std::thread([&ctrl_c, &stop, &walletBackend = walletBackend]
        {
            shutdown(ctrl_c, stop, walletBackend);
        });

        /* Trigger the shutdown signal if ctrl+c is used
           We do the actual handling in a separate thread to handle stuff not
           being re-entrant. */
        Tools::SignalHandler::install([&ctrl_c] { ctrl_c = true; });

        /* Don't explicitly sync in foreground if it's a new wallet */
        if (sync)
        {
            syncWallet(walletBackend);
        }

        /* Init the transaction monitor */
        txMonitor = std::make_shared<TransactionMonitor>(walletBackend);

        /* Launch the transaction monitor in another thread */
        txMonitorThread = std::thread(&TransactionMonitor::start, txMonitor.get());

        /* Launch the wallet interface */
        mainLoop(walletBackend, txMonitor->getMutex());

        /* Cleanup the threads */
        cleanup(txMonitorThread, ctrlCWatcher, stop, txMonitor);
        
        std::cout << InformationMsg("\nSaving and shutting down...\n");

        /* Wallet backend destructor gets called here, which saves */
    }
    catch (const std::exception &e)
    {
        std::cout << WarningMsg("Unexpected error: " + std::string(e.what()))
                  << std::endl
                  << "Please report this error, and what you were doing to "
                  << "cause it." << std::endl;

        std::cout << "Hit enter to exit: ";

        getchar();

        /* Cleanup the threads */
        cleanup(txMonitorThread, ctrlCWatcher, stop, txMonitor);
    }
        
    std::cout << "Thanks for stopping by..." << std::endl;
}
