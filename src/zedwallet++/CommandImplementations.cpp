// Portions Copyright (c) 2018-2019, The Catalyst Developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

///////////////////////////////////////////////
#include <zedwallet++/CommandImplementations.h>
///////////////////////////////////////////////

#include <config/WalletConfig.h>

#include <config/CryptoNoteConfig.h>

#include <Errors/ValidateParameters.h>

#include <fstream>

#include <Utilities/Addresses.h>
#include <Utilities/ColouredMsg.h>
#include <Utilities/FormatTools.h>
#include <Utilities/Input.h>
#include <Utilities/String.h>

#include <zedwallet++/Commands.h>
#include <zedwallet++/GetInput.h>
#include <zedwallet++/Menu.h>
#include <zedwallet++/Open.h>
#include <zedwallet++/Sync.h>
#include <zedwallet++/Utilities.h>

void changePassword(const std::shared_ptr<WalletBackend> walletBackend)
{
    /* Check the user knows the current password */
    ZedUtilities::confirmPassword(walletBackend, "Confirm your current password: ");

    /* Get a new password for the wallet */
    const std::string newPassword
        = getWalletPassword(true, "Enter your new password: ");

    /* Change the wallet password */
    Error error = walletBackend->changePassword(newPassword);

    if (error)
    {
        std::cout << WarningMsg("Your password has been changed, but saving "
                                "the updated wallet failed. If you quit without "
                                "saving succeeding, your password may not "
                                "update.") << std::endl;
    }
    else
    {
        std::cout << SuccessMsg("Your password has been changed!") << std::endl;
    }
}

void backup(const std::shared_ptr<WalletBackend> walletBackend)
{
    ZedUtilities::confirmPassword(walletBackend, "Confirm your current password: ");
    printPrivateKeys(walletBackend);
}

void printPrivateKeys(const std::shared_ptr<WalletBackend> walletBackend)
{
    const auto [privateSpendKey, privateViewKey] = walletBackend->getPrimaryAddressPrivateKeys();

    const auto [error, mnemonicSeed] = walletBackend->getMnemonicSeed();

    /* If this isn't a view only wallet, print out the spend key and mnemonic if available */
    if (!walletBackend->isViewWallet())
    {
        std::cout << SuccessMsg("\nPrivate spend key:\n")
                  << SuccessMsg(privateSpendKey) << "\n";
    }

    std::cout << SuccessMsg("Private view key:\n")
              << SuccessMsg(privateViewKey) << "\n";

    if (!error)
    {
        std::cout << SuccessMsg("\nMnemonic seed:\n")
                  << SuccessMsg(mnemonicSeed) << "\n";
    }
}

void balance(const std::shared_ptr<WalletBackend> walletBackend)
{
    auto [unlockedBalance, lockedBalance] = walletBackend->getTotalBalance();

    /* We can make a better approximation of the view wallet balance if we
       ignore fusion transactions.
       See https://github.com/turtlecoin/turtlecoin/issues/531 */
    if (walletBackend->isViewWallet())
    {
        unlockedBalance = 0;

        const auto transactions = walletBackend->getTransactions();

        for (const auto tx : transactions)
        {
            if (!tx.isFusionTransaction())
            {
                unlockedBalance += tx.totalAmount();
            }
        }
    }

    const uint64_t totalBalance = unlockedBalance + lockedBalance;

    std::cout << "Available balance: "
              << SuccessMsg(Utilities::formatAmount(unlockedBalance)) << "\n"
              << "Locked (unconfirmed) balance: "
              << WarningMsg(Utilities::formatAmount(lockedBalance))
              << "\nTotal balance: "
              << InformationMsg(Utilities::formatAmount(totalBalance)) << "\n";

    if (walletBackend->isViewWallet())
    {
        std::cout << InformationMsg("\nPlease note that view only wallets "
                                    "can only track incoming transactions,\n")
                  << InformationMsg("and so your wallet balance may appear "
                                    "inflated.\n");
    }

    const auto [walletBlockCount, localDaemonBlockCount, networkBlockCount]
        = walletBackend->getSyncStatus();

    if (localDaemonBlockCount < networkBlockCount)
    {
        std::cout << InformationMsg("\nYour daemon is not fully synced with "
                                    "the network!\n")
                  << "Your balance may be incorrect until you are fully "
                  << "synced!\n";
    }
    /* Small buffer because wallet height doesn't update instantly like node
       height does */
    else if (walletBlockCount + 1000 < networkBlockCount)
    {
        std::cout << InformationMsg("\nThe blockchain is still being scanned for "
                                    "your transactions.\n")
                  << "Balances might be incorrect whilst this is ongoing.\n";
    }
}

void printHeights(
    const uint64_t localDaemonBlockCount,
    const uint64_t networkBlockCount,
    const uint64_t walletBlockCount)
{
    /* This is the height that the wallet has been scanned to. The blockchain
       can be fully updated, but we have to walk the chain to find our
       transactions, and this number indicates that progress. */
    std::cout << "Wallet blockchain height: ";

    /* Small buffer because wallet height doesn't update instantly like node
       height does */
    if (walletBlockCount + 1000 > networkBlockCount)
    {
        std::cout << SuccessMsg(walletBlockCount);
    }
    else
    {
        std::cout << WarningMsg(walletBlockCount);
    }

    std::cout << "\nLocal blockchain height: ";

    if (localDaemonBlockCount == networkBlockCount)
    {
        std::cout << SuccessMsg(localDaemonBlockCount);
    }
    else
    {
        std::cout << WarningMsg(localDaemonBlockCount);
    }

    std::cout << "\nNetwork blockchain height: "
              << SuccessMsg(networkBlockCount) << "\n";
}

void printSyncStatus(
    const uint64_t localDaemonBlockCount,
    const uint64_t networkBlockCount,
    const uint64_t walletBlockCount)
{
    std::string networkSyncPercentage
        = Utilities::get_sync_percentage(localDaemonBlockCount, networkBlockCount) + "%";

    std::string walletSyncPercentage
        = Utilities::get_sync_percentage(walletBlockCount, networkBlockCount) + "%";

    std::cout << "Network sync status: ";

    if (localDaemonBlockCount == networkBlockCount)
    {
        std::cout << SuccessMsg(networkSyncPercentage) << std::endl;
    }
    else
    {
        std::cout << WarningMsg(networkSyncPercentage) << std::endl;
    }

    std::cout << "Wallet sync status: ";
    
    /* Small buffer because wallet height is not always completely accurate */
    if (walletBlockCount + 10 > networkBlockCount)
    {
        std::cout << SuccessMsg(walletSyncPercentage) << std::endl;
    }
    else
    {
        std::cout << WarningMsg(walletSyncPercentage) << std::endl;
    }
}

void printSyncSummary(
    const uint64_t localDaemonBlockCount,
    const uint64_t networkBlockCount,
    const uint64_t walletBlockCount)
{
    if (localDaemonBlockCount == 0 && networkBlockCount == 0)
    {
        std::cout << WarningMsg("Uh oh, it looks like you don't have ")
                  << WarningMsg(WalletConfig::daemonName)
                  << WarningMsg(" open!")
                  << std::endl;
    }
    else if (walletBlockCount + 1000 < networkBlockCount && localDaemonBlockCount == networkBlockCount)
    {
        std::cout << InformationMsg("You are synced with the network, but the "
                                    "blockchain is still being scanned for "
                                    "your transactions.")
                  << std::endl
                  << "Balances might be incorrect whilst this is ongoing."
                  << std::endl;
    }
    else if (localDaemonBlockCount == networkBlockCount)
    {
        std::cout << SuccessMsg("Yay! You are synced!") << std::endl;
    }
    else
    {
        std::cout << WarningMsg("Be patient, you are still syncing with the "
                                "network!") << std::endl;
    }
}

void printHashrate(const uint64_t hashrate)
{
    /* Offline node / not responding */
    if (hashrate == 0)
    {
        return;
    }

    std::cout << "Network hashrate: "
              << SuccessMsg(Utilities::get_mining_speed(hashrate))
              << " (Based on the last local block)" << std::endl;
}

void status(const std::shared_ptr<WalletBackend> walletBackend)
{
    const WalletTypes::WalletStatus status = walletBackend->getStatus();

    /* Print the heights of local, remote, and wallet */
    printHeights(
        status.localDaemonBlockCount, status.networkBlockCount,
        status.walletBlockCount
    );

    std::cout << "\n";

    /* Print the network and wallet sync status in percentage */
    printSyncStatus(
        status.localDaemonBlockCount, status.networkBlockCount,
        status.walletBlockCount
    );

    std::cout << "\n";

    /* Print the network hashrate, based on the last local block */
    printHashrate(status.lastKnownHashrate);

    /* Print the amount of peers we have */
    std::cout << "Peers: " << SuccessMsg(status.peerCount) << "\n\n";

    /* Print a summary of the sync status */
    printSyncSummary(
        status.localDaemonBlockCount, status.networkBlockCount,
        status.walletBlockCount
    );
}

void reset(const std::shared_ptr<WalletBackend> walletBackend)
{
    const uint64_t scanHeight = ZedUtilities::getScanHeight();

    std::cout << std::endl
              << InformationMsg("This process may take some time to complete.")
              << std::endl
              << InformationMsg("You can't make any transactions during the ")
              << InformationMsg("process.")
              << std::endl << std::endl;
    
    if (!Utilities::confirm("Are you sure?"))
    {
        return;
    }
    
    std::cout << InformationMsg("Resetting wallet...") << std::endl;

    const uint64_t timestamp = 0;

    /* Don't want to queue up transaction events, since sync wallet will print
       them out */
    walletBackend->m_eventHandler->onTransaction.pause();

    walletBackend->reset(scanHeight, timestamp);

    syncWallet(walletBackend);

    /* Readd the event handler for new events */
    walletBackend->m_eventHandler->onTransaction.resume();
}

void saveCSV(const std::shared_ptr<WalletBackend> walletBackend)
{
    const auto transactions = walletBackend->getTransactions();

    if (transactions.empty())
    {
        std::cout << WarningMsg("You have no transactions to save to the CSV!\n");
        return;
    }

    std::ofstream csv(WalletConfig::csvFilename);

    if (!csv)
    {
        std::cout << WarningMsg("Couldn't open transactions.csv file for "
                                "saving!")
                  << std::endl
                  << WarningMsg("Ensure it is not open in any other "
                                "application.")
                  << std::endl;
        return;
    }

    std::cout << InformationMsg("Saving CSV file...") << std::endl;

    /* Create CSV header */
    csv << "Timestamp,Block Height,Hash,Amount,In/Out"
        << std::endl;
    
    for (const auto tx : transactions)
    {
        /* Ignore fusion transactions */
        if (tx.isFusionTransaction())
        {
            continue;
        }

        const std::string amount = Utilities::formatAmountBasic(std::abs(tx.totalAmount()));

        const std::string direction = tx.totalAmount() > 0 ? "IN" : "OUT";

        csv << Utilities::unixTimeToDate(tx.timestamp) << ","    /* Timestamp */
            << tx.blockHeight << ","                                /* Block Height */
            << tx.hash << ","                                       /* Hash */
            << amount << ","                                        /* Amount */
            << direction                                            /* In/Out */
            << std::endl;
    }

    std::cout << SuccessMsg("CSV successfully written to ")
              << SuccessMsg(WalletConfig::csvFilename)
              << SuccessMsg("!")
              << std::endl;
}

void printOutgoingTransfer(const WalletTypes::Transaction tx)
{
    std::stringstream stream;

    const int64_t amount = std::abs(tx.totalAmount());

    stream << "Outgoing transfer:\nHash: " << tx.hash << "\n";

    /* These will not be initialized for outgoing, unconfirmed transactions */
    if (tx.blockHeight != 0 && tx.timestamp != 0)
    {
        stream << "Block height: " << tx.blockHeight << "\n"
               << "Timestamp: " << Utilities::unixTimeToDate(tx.timestamp) << "\n";
    }

    stream << "Spent: " << Utilities::formatAmount(amount - tx.fee) << "\n"
           << "Fee: " << Utilities::formatAmount(tx.fee) << "\n"
           << "Total Spent: " << Utilities::formatAmount(amount) << "\n";

    if (tx.paymentID != "")
    {
        stream << "Payment ID: " << tx.paymentID << "\n";
    }

    std::cout << WarningMsg(stream.str()) << std::endl;
}

void printIncomingTransfer(const WalletTypes::Transaction tx)
{
    std::stringstream stream;

    const int64_t amount = tx.totalAmount();

    stream << "Incoming transfer:\nHash: " << tx.hash << "\n"
           << "Block height: " << tx.blockHeight << "\n"
           << "Timestamp: " << Utilities::unixTimeToDate(tx.timestamp) << "\n"
           << "Amount: " << Utilities::formatAmount(amount) << "\n";

    if (tx.paymentID != "")
    {
        stream << "Payment ID: " << tx.paymentID << "\n";
    }

    /* Display Unlock time, if applicable; otherwise, don't */
    int64_t difference = tx.unlockTime - tx.blockHeight;

    /* Here we treat Unlock as a block, and treat it that way in the future */
    if (tx.unlockTime != 0 && difference > 0 && tx.unlockTime < CryptoNote::parameters::CRYPTONOTE_MAX_BLOCK_NUMBER)
    {
        int64_t unlockInUnixTime = tx.timestamp + (difference * CryptoNote::parameters::DIFFICULTY_TARGET);

        std::cout << SuccessMsg(stream.str())
                  << InformationMsg("Unlock height: ")
                  << InformationMsg(tx.unlockTime)
                  << std::endl
                  << InformationMsg("Unlocks at approximately: ")
                  << InformationMsg(Utilities::unixTimeToDate(unlockInUnixTime))
                  << std::endl
                  << std::endl;
    }
    /* Here we treat Unlock as Unix time, and treat it that way in the future */
    else if (tx.unlockTime > std::time(nullptr))
    {
        std::cout << SuccessMsg(stream.str())
                  << InformationMsg("Unlocks at: ")
                  << InformationMsg(Utilities::unixTimeToDate(tx.unlockTime))
                  << std::endl
                  << std::endl;
    }
    else
    {
        std::cout << SuccessMsg(stream.str()) << std::endl;
    }
}

void listTransfers(
    const bool incoming,
    const bool outgoing, 
    const std::shared_ptr<WalletBackend> walletBackend)
{
    uint64_t totalSpent = 0;
    uint64_t totalReceived = 0;

    uint64_t numIncomingTransactions = 0;
    uint64_t numOutgoingTransactions = 0;

    /* Grab confirmed transactions */
    std::vector<WalletTypes::Transaction> transactions = walletBackend->getTransactions();

    /* Grab any outgoing transactions still in the pool */
    const auto unconfirmedTransactions = walletBackend->getUnconfirmedTransactions();

    /* Append them, unconfirmed transactions last */
    transactions.insert(transactions.end(), unconfirmedTransactions.begin(),
                        unconfirmedTransactions.end());

    for (const auto tx : transactions) 
    {
        /* Is a fusion transaction (on a view only wallet). It appears to have
           an incoming amount, because we can't detract the outputs (can't
           decrypt them) */
        if (tx.isFusionTransaction())
        {
            continue;
        }

        const int64_t amount = tx.totalAmount();

        if (amount < 0 && outgoing)
        {
            printOutgoingTransfer(tx);

            totalSpent += -amount;
            numOutgoingTransactions++;
        }
        else if (amount > 0 && incoming)
        {
            printIncomingTransfer(tx);

            totalReceived += amount;
            numIncomingTransactions++;
        }
    }

    std::cout << InformationMsg("Summary:\n\n");

    if (incoming)
    {
        std::cout << SuccessMsg(numIncomingTransactions)
                  << SuccessMsg(" incoming transactions, totalling ")
                  << SuccessMsg(Utilities::formatAmount(totalReceived))
                  << std::endl;
    }

    if (outgoing)
    {
        std::cout << WarningMsg(numOutgoingTransactions)
                  << WarningMsg(" outgoing transactions, totalling ")
                  << WarningMsg(Utilities::formatAmount(totalSpent))
                  << std::endl;
    }
}

void save(const std::shared_ptr<WalletBackend> walletBackend)
{
    std::cout << InformationMsg("Saving.") << std::endl;

    Error error = walletBackend->save();

    if (error)
    {
        std::cout << WarningMsg("Failed to save wallet! Error: ")
                  << WarningMsg(error) << std::endl;
    }
    else
    {
        std::cout << InformationMsg("Saved.") << std::endl;
    }
}

void createIntegratedAddress()
{
    std::cout << InformationMsg("Creating an integrated address from an ")
              << InformationMsg("address and payment ID pair...")
              << std::endl << std::endl;

    std::string address;
    std::string paymentID;

    while (true)
    {
        std::cout << InformationMsg("Address: ");

        std::getline(std::cin, address);

        Utilities::trim(address);

        const bool integratedAddressesAllowed = false;

        if (Error error = validateAddresses({address}, integratedAddressesAllowed); error != SUCCESS)
        {
            std::cout << WarningMsg("Invalid address: ")
                      << WarningMsg(error) << std::endl;
        }
        else
        {
            break;
        }
    }

    while (true)
    {
        std::cout << InformationMsg("Payment ID: ");

        std::getline(std::cin, paymentID);

        Utilities::trim(paymentID);

        /* Validate the payment ID */
        if (Error error = validatePaymentID(paymentID); error != SUCCESS)
        {
            std::cout << WarningMsg("Invalid payment ID: ")
                      << WarningMsg(error) << std::endl;
        }
        else
        {
            break;
        }
    }

    const auto [error, integratedAddress] = Utilities::createIntegratedAddress(
        address, paymentID
    );

    /* Shouldn't happen, but lets check anyway */
    if (error)
    {
        std::cout << WarningMsg("Failed to create integrated address: ")
                  << WarningMsg(error) << std::endl;
    }
    else
    {
        std::cout << InformationMsg(integratedAddress) << std::endl;
    }
}

void help(const std::shared_ptr<WalletBackend> walletBackend)
{
    if (walletBackend->isViewWallet())
    {
        printCommands(basicViewWalletCommands());
    }
    else
    {
        printCommands(basicCommands());
    }
}

void advanced(const std::shared_ptr<WalletBackend> walletBackend)
{
    /* We pass the offset of the command to know what index to print for
       command numbers */
    if (walletBackend->isViewWallet())
    {
        printCommands(advancedViewWalletCommands(),
                      basicViewWalletCommands().size());
    }
    else
    {
        printCommands(advancedCommands(),
                      basicCommands().size());
    }
}

void swapNode(const std::shared_ptr<WalletBackend> walletBackend)
{
    const auto [host, port, ssl] = getDaemonAddress();

    std::cout << InformationMsg("\nSwapping node, this may take some time...\n");

    walletBackend->swapNode(host, port, ssl);

    std::cout << SuccessMsg("Node swap complete.\n\n");
}

void getTxPrivateKey(const std::shared_ptr<WalletBackend> walletBackend)
{
    const std::string txHash = getHash(
        "What transaction hash do you want to get the private key of?: ", true
    );

    if (txHash == "cancel")
    {
        return;
    }

    Crypto::Hash hash;

    Common::podFromHex(txHash, hash);

    const auto [error, key] = walletBackend->getTxPrivateKey(hash);

    if (error)
    {
        std::cout << WarningMsg(error) << std::endl;
    }
    else
    {
        std::cout << InformationMsg("Transaction private key: ")
                  << SuccessMsg(key) << std::endl;
    }
}
