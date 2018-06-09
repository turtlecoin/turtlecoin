/*
Copyright (C) 2018, The TurtleCoin developers

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

//////////////////////////////
#include <SimpleWallet/Sync.h>
//////////////////////////////

#include <Common/StringTools.h>

#include <memory>

#include <SimpleWallet/Tools.h>
#include <SimpleWallet/Types.h>

CryptoNote::BlockDetails getBlock(uint32_t blockHeight,
                                  CryptoNote::INode &node)
{
    CryptoNote::BlockDetails block;

    /* No connection to turtlecoind */
    if (node.getLastKnownBlockHeight() == 0)
    {
        return block;
    }

    std::promise<std::error_code> errorPromise;

    auto e = errorPromise.get_future();

    auto callback = [&errorPromise](std::error_code e)
    {
        errorPromise.set_value(e);
    };

    node.getBlock(blockHeight, block, callback);

    if (e.get())
    {
        /* Prevent the compiler optimizing it out... */
        std::cout << "";
    }

    return block;
}

std::string getBlockTimestamp(CryptoNote::BlockDetails b)
{
    if (b.timestamp == 0)
    {
        return "";
    }

    std::time_t time = b.timestamp;
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%F %R", std::localtime(&time));
    return std::string(buffer);
}

void printOutgoingTransfer(CryptoNote::WalletTransaction t,
                           CryptoNote::INode &node)
{
    std::string blockTime = getBlockTimestamp(getBlock(t.blockHeight, node));

    std::cout << WarningMsg("Outgoing transfer:")
              << std::endl
              << WarningMsg("Hash: " + Common::podToHex(t.hash))
              << std::endl
              << WarningMsg("Spent: " + formatAmount(-t.totalAmount - t.fee))
              << std::endl
              << WarningMsg("Fee: " + formatAmount(t.fee))
              << std::endl
              << WarningMsg("Total Spent: " + formatAmount(-t.totalAmount))
              << std::endl;

    std::string paymentID = getPaymentID(t.extra);

    if (paymentID != "")
    {
        std::cout << WarningMsg("Payment ID: " + paymentID) << std::endl;
    }

    /* Couldn't get timestamp, maybe old node or turtlecoind closed */
    if (blockTime != "")
    {
        std::cout << WarningMsg("Timestamp: " + blockTime) << std::endl;
    }

    std::cout << std::endl;
}

void printIncomingTransfer(CryptoNote::WalletTransaction t,
                           CryptoNote::INode &node)
{
    std::string blockTime = getBlockTimestamp(getBlock(t.blockHeight, node));

    std::cout << SuccessMsg("Incoming transfer:")
              << std::endl
              << SuccessMsg("Hash: " + Common::podToHex(t.hash))
              << std::endl
              << SuccessMsg("Amount: " + formatAmount(t.totalAmount))
              << std::endl;

    std::string paymentID = getPaymentID(t.extra);

    if (paymentID != "")
    {
        std::cout << SuccessMsg("Payment ID: " + paymentID) << std::endl;
    }

    /* Couldn't get timestamp, maybe old node or turtlecoind closed */
    if (blockTime != "")
    {
        std::cout << SuccessMsg("Timestamp: " + blockTime) << std::endl;
    }

    std::cout << std::endl;
}

void listTransfers(bool incoming, bool outgoing, 
                   CryptoNote::WalletGreen &wallet, CryptoNote::INode &node)
{
    size_t numTransactions = wallet.getTransactionCount();
    int64_t totalSpent = 0;
    int64_t totalReceived = 0;

    for (size_t i = 0; i < numTransactions; i++)
    {
        CryptoNote::WalletTransaction t = wallet.getTransaction(i);

        if (t.totalAmount < 0 && outgoing)
        {
            printOutgoingTransfer(t, node);
            totalSpent += -t.totalAmount;
        }
        else if (t.totalAmount > 0 && incoming)
        {
            printIncomingTransfer(t, node);
            totalReceived += t.totalAmount;
        }
    }

    if (incoming)
    {
        std::cout << SuccessMsg("Total received: " 
                              + formatAmount(totalReceived))
                  << std::endl;
    }

    if (outgoing)
    {
        std::cout << WarningMsg("Total spent: " + formatAmount(totalSpent))
                  << std::endl;
    }
}

void saveCSV(CryptoNote::WalletGreen &wallet, CryptoNote::INode &node)
{
    /* oaf: this routine saves transactions to local CSV file */
    size_t numTransactions = wallet.getTransactionCount();

    std::ofstream myfile;
    myfile.open("transactions.csv");
    if (!myfile)
    {
        std::cout << WarningMsg("Couldn't open transactions.csv file for saving!")
                  << std::endl;
        std::cout << WarningMsg("Ensure it is not open in any other application.")
                  << std::endl;
        return;
    }
    std::cout << InformationMsg("Saving CSV file...") << std::endl;

    /* Create header line for CSV file */
    myfile << "Block date/time,Block Height,Hash,Amount,Currency,In/Out\n";
    /* Loop through transactions */
    for (size_t i = 0; i < numTransactions; i++)
    {
        CryptoNote::WalletTransaction t = wallet.getTransaction(i);
        /* Ignore fusion transactions */
        if (t.totalAmount != 0) {
            std::string blockTime = getBlockTimestamp(getBlock(t.blockHeight, node));
            myfile << blockTime << "," << t.blockHeight << ","
                   << Common::podToHex(t.hash) << ",";
            /* Handle outgoing (negative) or incoming transactions */
            if (t.totalAmount < 0)
            {
                /* Put TRTL in separate field, makes output more usable in spreadsheet */
                std::string splitAmtTRTL = formatAmount(-t.totalAmount);
                boost::replace_all(splitAmtTRTL, " ", ",");
                myfile << "-" << splitAmtTRTL << ",OUT\n";
            }
            else
            {
                std::string splitAmtTRTL = formatAmount(t.totalAmount);
                boost::replace_all(splitAmtTRTL, " ", ",");
                myfile << splitAmtTRTL << ",IN\n";
            }
        }
    }
    myfile.close();
    std::cout << SuccessMsg("CSV file saved successfully.")
              << std::endl;
}

void checkForNewTransactions(std::shared_ptr<WalletInfo> &walletInfo)
{
    walletInfo->wallet.updateInternalCache();

    size_t newTransactionCount = walletInfo->wallet.getTransactionCount();

    if (newTransactionCount != walletInfo->knownTransactionCount)
    {
        for (size_t i = walletInfo->knownTransactionCount; 
                    i < newTransactionCount; i++)
        {
            CryptoNote::WalletTransaction t 
                = walletInfo->wallet.getTransaction(i);

            /* Don't print outgoing or fusion transfers */
            if (t.totalAmount > 0)
            {
                std::cout << std::endl
                          << InformationMsg("New transaction found!")
                          << std::endl
                          << SuccessMsg("Incoming transfer:")
                          << std::endl
                          << SuccessMsg("Hash: " + Common::podToHex(t.hash))
                          << std::endl
                          << SuccessMsg("Amount: "
                                      + formatAmount(t.totalAmount))
                          << std::endl
                          << getPrompt(walletInfo)
                          << std::flush;
            }
        }

        walletInfo->knownTransactionCount = newTransactionCount;
    }
}

void syncWallet(CryptoNote::INode &node,
                std::shared_ptr<WalletInfo> &walletInfo)
{
    uint32_t localHeight = node.getLastLocalBlockHeight();
    uint32_t walletHeight = walletInfo->wallet.getBlockCount();
    uint32_t remoteHeight = node.getLastKnownBlockHeight();

    size_t transactionCount = walletInfo->wallet.getTransactionCount();

    int stuckCounter = 0;

    if (localHeight != remoteHeight)
    {
        std::cout << "Your TurtleCoind isn't fully synced yet!" << std::endl
                  << "Until you are fully synced, you won't be able to send "
                  << "transactions,"
                  << std::endl
                  << "and your balance may be missing or "
                  << "incorrect!" << std::endl << std::endl;
    }

    /* If we open a legacy wallet then it will load the transactions but not
       have the walletHeight == transaction height. Lets just throw away the
       transactions and rescan. */
    if (walletHeight == 1 && transactionCount != 0)
    {
        std::cout << "Upgrading your wallet from an older version of the "
                  << "software..." << std::endl << "Unfortunately, we have "
                  << "to rescan the chain to find your transactions."
                  << std::endl;
        transactionCount = 0;
        walletInfo->wallet.clearCaches(true, false);
    }

    if (walletHeight == 1)
    {
        std::cout << "Scanning through the blockchain to find transactions "
                  << "that belong to you." << std::endl
                  << "Please wait, this will take some time."
                  << std::endl << std::endl;
    }
    else
    {
        std::cout << "Scanning through the blockchain to find any new "
                  << "transactions you received"
                  << std::endl
                  << "whilst your wallet wasn't open."
                  << std::endl
                  << "Please wait, this may take some time."
                  << std::endl << std::endl;
    }

    while (walletHeight < localHeight)
    {
        int counter = 1;

        /* This MUST be called on the main thread! */
        walletInfo->wallet.updateInternalCache();

        localHeight = node.getLastLocalBlockHeight();
        remoteHeight = node.getLastKnownBlockHeight();
        std::cout << SuccessMsg(std::to_string(walletHeight))
                  << " of " << InformationMsg(std::to_string(localHeight))
                  << std::endl;

        uint32_t tmpWalletHeight = walletInfo->wallet.getBlockCount();

        int waitSeconds = 1;

        /* Save periodically so if someone closes before completion they don't
           lose all their progress */
        if (counter % 60 == 0)
        {
            walletInfo->wallet.save();
        }

        if (tmpWalletHeight == walletHeight)
        {
            stuckCounter++;
            waitSeconds = 3;

            if (stuckCounter > 20)
            {
                std::string warning =
                    "Syncing may be stuck. Try restarting Turtlecoind.\n"
                    "If this persists, visit "
                    "https://turtlecoin.lol/#contact for support.";
                std::cout << WarningMsg(warning) << std::endl;
            }
            else if (stuckCounter > 19)
            {
                /*
                   Calling save has the side-effect of starting
                   and stopping blockchainSynchronizer, which seems
                   to sometimes force the sync to resume properly.
                   So we'll try this before warning the user.
                */
                std::cout << InformationMsg("Saving wallet.") << std::endl;
                walletInfo->wallet.save();
                waitSeconds = 5;
            }
        }
        else
        {
            stuckCounter = 0;
            walletHeight = tmpWalletHeight;

            size_t tmpTransactionCount = walletInfo
                                       ->wallet.getTransactionCount();

            if (tmpTransactionCount != transactionCount)
            {
                for (size_t i = transactionCount; i < tmpTransactionCount; i++)
                {
                    CryptoNote::WalletTransaction t
                        = walletInfo->wallet.getTransaction(i);

                    /* Don't print out fusion transactions */
                    if (t.totalAmount != 0)
                    {
                        std::cout << std::endl
                                  << InformationMsg("New transaction found!")
                                  << std::endl << std::endl;

                        if (t.totalAmount < 0)
                        {
                            printOutgoingTransfer(t, node);
                        }
                        else
                        {
                            printIncomingTransfer(t, node);
                        }
                    }
                }

                transactionCount = tmpTransactionCount;
            }
        }

        counter++;

        std::this_thread::sleep_for(std::chrono::seconds(waitSeconds));
    }

    std::cout << std::endl
              << SuccessMsg("Finished scanning blockchain!") << std::endl
              << std::endl;

    /* In case the user force closes, we don't want them to have to rescan
       the whole chain. */
    walletInfo->wallet.save();

    walletInfo->knownTransactionCount = transactionCount;
}

ColouredMsg getPrompt(std::shared_ptr<WalletInfo> &walletInfo)
{
    const int promptLength = 20;
    const std::string extension = ".wallet";

    std::string walletName = walletInfo->walletFileName;

    /* Filename ends in .wallet, remove extension */
    if (std::equal(extension.rbegin(), extension.rend(), 
                   walletInfo->walletFileName.rbegin()))
    {
        size_t extPos = walletInfo->walletFileName.find_last_of('.');

        walletName = walletInfo->walletFileName.substr(0, extPos);
    }

    std::string shortName = walletName.substr(0, promptLength);

    return InformationMsg("[TRTL " + shortName + "]: ");
}
