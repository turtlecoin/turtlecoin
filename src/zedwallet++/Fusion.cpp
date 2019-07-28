// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

///////////////////////////////
#include <zedwallet++/Fusion.h>
///////////////////////////////

#include <iostream>
#include <utilities/ColouredMsg.h>
#include <utilities/FormatTools.h>
#include <walletbackend/WalletBackend.h>
#include <zedwallet++/Utilities.h>

void optimize(const std::shared_ptr<WalletBackend> walletBackend)
{
    int optimizationRound = 1;

    while (true)
    {
        std::stringstream stream;

        stream << "Running optimization round " << optimizationRound << "...\n\n";

        std::cout << InformationMsg(stream.str());

        /* No optimization done on this round, we're done */
        if (!optimizeRound(walletBackend))
        {
            break;
        }

        optimizationRound++;
    }

    std::cout << SuccessMsg("Optimization complete!\n");
}

bool optimizeRound(const std::shared_ptr<WalletBackend> walletBackend)
{
    int failCount = 0;

    int sentTransactions = 0;

    const uint64_t initialBalance = walletBackend->getTotalUnlockedBalance();

    /* Since input selection is random, lets let it fail a few times before
       failing the whole round */
    while (failCount < 5)
    {
        const auto [error, hash] = walletBackend->sendFusionTransactionBasic();

        if (error == FULLY_OPTIMIZED)
        {
            failCount++;
        }
        else if (error)
        {
            failCount++;

            std::stringstream stream;

            stream << "Failed to send fusion transaction: " << error << "\n";

            std::cout << WarningMsg(stream.str());
        }
        else
        {
            failCount = 0;

            sentTransactions++;

            std::cout << InformationMsg("Sent fusion transaction #") << InformationMsg(sentTransactions)
                      << SuccessMsg("\nHash: ") << SuccessMsg(hash) << "\n\n";
        }
    }

    uint64_t currentBalance = walletBackend->getTotalUnlockedBalance();

    /* Wait for balance to unlock, so sending transactions can proceed */
    while (currentBalance < initialBalance)
    {
        std::cout << InformationMsg("Waiting for balance to return and unlock:\n"
                                    "\nTotal balance: ")
                  << InformationMsg(Utilities::formatAmount(initialBalance))

                  << WarningMsg("\nLocked balance: ")
                  << WarningMsg(Utilities::formatAmount(initialBalance - currentBalance))

                  << SuccessMsg("\nUnlocked balance: ") << SuccessMsg(Utilities::formatAmount(currentBalance))

                  << InformationMsg("\nWill check again in 15 seconds...\n\n");

        std::this_thread::sleep_for(std::chrono::seconds(15));

        currentBalance = walletBackend->getTotalUnlockedBalance();
    }

    if (sentTransactions != 0)
    {
        std::cout << SuccessMsg("All fusion transactions confirmed!\n\n");
    }

    /* Return whether we sent any transactions or not. If we did, we will
       probably start another round */
    return sentTransactions != 0;
}
