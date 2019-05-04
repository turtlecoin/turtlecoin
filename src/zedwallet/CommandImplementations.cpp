// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

/////////////////////////////////////////////
#include <zedwallet/CommandImplementations.h>
/////////////////////////////////////////////

#include <atomic>

#include <Common/StringTools.h>

#include <config/WalletConfig.h>

#include <CryptoNoteCore/Account.h>
#include <Common/TransactionExtra.h>

#ifndef MSVC
#include <fstream>
#endif

#include <Mnemonics/Mnemonics.h>

#include <Utilities/FormatTools.h>

#include <zedwallet/AddressBook.h>
#include <Utilities/ColouredMsg.h>
#include <zedwallet/Commands.h>
#include <zedwallet/Fusion.h>
#include <zedwallet/Menu.h>
#include <zedwallet/Open.h>
#include <zedwallet/Sync.h>
#include <zedwallet/Tools.h>
#include <zedwallet/Transfer.h>
#include <zedwallet/Types.h>

void changePassword(std::shared_ptr<WalletInfo> walletInfo)
{
    /* Check the user knows the current password */
    confirmPassword(walletInfo->walletPass, "Confirm your current password: ");

    /* Get a new password for the wallet */
    const std::string newPassword
        = getWalletPassword(true, "Enter your new password: ");

    /* Change the wallet password */
    walletInfo->wallet.changePassword(walletInfo->walletPass, newPassword);

    /* Change the stored wallet metadata */
    walletInfo->walletPass = newPassword;

    /* Make sure we save with the new password */
    walletInfo->wallet.save();

    std::cout << SuccessMsg("Your password has been changed!") << std::endl;
}

void exportKeys(std::shared_ptr<WalletInfo> walletInfo)
{
    confirmPassword(walletInfo->walletPass);
    printPrivateKeys(walletInfo->wallet, walletInfo->viewWallet);
}

void printPrivateKeys(CryptoNote::WalletGreen &wallet, bool viewWallet)
{
    const Crypto::SecretKey privateViewKey = wallet.getViewKey().secretKey;

    if (viewWallet)
    {
        std::cout << SuccessMsg("Private view key:")
                  << std::endl
                  << SuccessMsg(Common::podToHex(privateViewKey))
                  << std::endl;
        return;
    }

    Crypto::SecretKey privateSpendKey = wallet.getAddressSpendKey(0).secretKey;

    Crypto::SecretKey derivedPrivateViewKey;

    Crypto::crypto_ops::generateViewFromSpend(privateSpendKey,
                                                   derivedPrivateViewKey);

    const bool deterministicPrivateKeys
             = derivedPrivateViewKey == privateViewKey;

    std::cout << SuccessMsg("Private spend key:")
              << std::endl
              << SuccessMsg(Common::podToHex(privateSpendKey))
              << std::endl
              << std::endl
              << SuccessMsg("Private view key:")
              << std::endl
              << SuccessMsg(Common::podToHex(privateViewKey))
              << std::endl;

    if (deterministicPrivateKeys)
    {
        std::cout << std::endl
                  << SuccessMsg("Mnemonic seed:")
                  << std::endl
                  << SuccessMsg(Mnemonics::PrivateKeyToMnemonic(privateSpendKey))
                  << std::endl;
    }
}

void balance(CryptoNote::INode &node, CryptoNote::WalletGreen &wallet,
             bool viewWallet)
{
    const uint64_t unconfirmedBalance = wallet.getPendingBalance();
    uint64_t confirmedBalance = wallet.getActualBalance();

    const uint32_t localHeight = node.getLastLocalBlockHeight();
    const uint32_t remoteHeight = node.getLastKnownBlockHeight();
    const uint32_t walletHeight = wallet.getBlockCount();

    /* We can make a better approximation of the view wallet balance if we
       ignore fusion transactions.
       See https://github.com/turtlecoin/turtlecoin/issues/531 */
    if (viewWallet)
    {
        /* Not sure how to verify if a transaction is unlocked or not via
           the WalletTransaction type, so this is technically not correct,
           we might be including locked balance. */
        confirmedBalance = 0;

        size_t numTransactions = wallet.getTransactionCount();
        
        for (size_t i = 0; i < numTransactions; i++)
        {
            const CryptoNote::WalletTransaction t = wallet.getTransaction(i);

            /* Fusion transactions are zero fee, skip them. Coinbase
               transactions are also zero fee, include them. */
            if (t.fee != 0 || t.isBase)
            {
                confirmedBalance += t.totalAmount;
            }
        }
    }

    const uint64_t totalBalance = unconfirmedBalance + confirmedBalance;

    std::cout << "Available balance: "
              << SuccessMsg(formatAmount(confirmedBalance)) << std::endl
              << "Locked (unconfirmed) balance: "
              << WarningMsg(formatAmount(unconfirmedBalance))
              << std::endl << "Total balance: "
              << InformationMsg(formatAmount(totalBalance)) << std::endl;

    if (viewWallet)
    {
        std::cout << std::endl 
                  << InformationMsg("Please note that view only wallets "
                                    "can only track incoming transactions,")
                  << std::endl
                  << InformationMsg("and so your wallet balance may appear "
                                    "inflated.") << std::endl;
    }

    if (localHeight < remoteHeight)
    {
        std::cout << std::endl
                  << InformationMsg("Your daemon is not fully synced with "
                                    "the network!")
                  << std::endl
                  << "Your balance may be incorrect until you are fully "
                  << "synced!" << std::endl;
    }
    /* Small buffer because wallet height doesn't update instantly like node
       height does */
    else if (walletHeight + 1000 < remoteHeight)
    {
        std::cout << std::endl
                  << InformationMsg("The blockchain is still being scanned for "
                                    "your transactions.")
                  << std::endl
                  << "Balances might be incorrect whilst this is ongoing."
                  << std::endl;
    }
}

void printHeights(uint32_t localHeight, uint32_t remoteHeight,
                  uint32_t walletHeight)
{
    /* This is the height that the wallet has been scanned to. The blockchain
       can be fully updated, but we have to walk the chain to find our
       transactions, and this number indicates that progress. */
    std::cout << "Wallet blockchain height: ";

    /* Small buffer because wallet height doesn't update instantly like node
       height does */
    if (walletHeight + 1000 > remoteHeight)
    {
        std::cout << SuccessMsg(std::to_string(walletHeight));
    }
    else
    {
        std::cout << WarningMsg(std::to_string(walletHeight));
    }

    std::cout << std::endl << "Local blockchain height: ";

    if (localHeight == remoteHeight)
    {
        std::cout << SuccessMsg(std::to_string(localHeight));
    }
    else
    {
        std::cout << WarningMsg(std::to_string(localHeight));
    }

    std::cout << std::endl << "Network blockchain height: "
              << SuccessMsg(std::to_string(remoteHeight)) << std::endl;
}

void printSyncStatus(uint32_t localHeight, uint32_t remoteHeight,
                     uint32_t walletHeight)
{
    std::string networkSyncPercentage
        = Utilities::get_sync_percentage(localHeight, remoteHeight) + "%";

    std::string walletSyncPercentage
        = Utilities::get_sync_percentage(walletHeight, remoteHeight) + "%";

    std::cout << "Network sync status: ";

    if (localHeight == remoteHeight)
    {
        std::cout << SuccessMsg(networkSyncPercentage) << std::endl;
    }
    else
    {
        std::cout << WarningMsg(networkSyncPercentage) << std::endl;
    }

    std::cout << "Wallet sync status: ";
    
    /* Small buffer because wallet height is not always completely accurate */
    if (walletHeight + 1000 > remoteHeight)
    {
        std::cout << SuccessMsg(walletSyncPercentage) << std::endl;
    }
    else
    {
        std::cout << WarningMsg(walletSyncPercentage) << std::endl;
    }
}

void printSyncSummary(uint32_t localHeight, uint32_t remoteHeight,
                      uint32_t walletHeight)
{
    if (localHeight == 0 && remoteHeight == 0)
    {
        std::cout << WarningMsg("Uh oh, it looks like you don't have ")
                  << WarningMsg(WalletConfig::daemonName)
                  << WarningMsg(" open!")
                  << std::endl;
    }
    else if (walletHeight + 1000 < remoteHeight && localHeight == remoteHeight)
    {
        std::cout << InformationMsg("You are synced with the network, but the "
                                    "blockchain is still being scanned for "
                                    "your transactions.")
                  << std::endl
                  << "Balances might be incorrect whilst this is ongoing."
                  << std::endl;
    }
    else if (localHeight == remoteHeight)
    {
        std::cout << SuccessMsg("Yay! You are synced!") << std::endl;
    }
    else
    {
        std::cout << WarningMsg("Be patient, you are still syncing with the "
                                "network!") << std::endl;
    }
}

void printPeerCount(size_t peerCount)
{
    std::cout << "Peers: " << SuccessMsg(std::to_string(peerCount))
              << std::endl;
}

void printHashrate(uint64_t difficulty)
{
    /* Offline node / not responding */
    if (difficulty == 0)
    {
        return;
    }

    /* Hashrate is difficulty divided by block target time */
    uint32_t hashrate = static_cast<uint32_t>(
        round(difficulty / CryptoNote::parameters::DIFFICULTY_TARGET)
    );

    std::cout << "Network hashrate: "
              << SuccessMsg(Utilities::get_mining_speed(hashrate))
              << " (Based on the last local block)" << std::endl;
}

/* This makes sure to call functions on the node which only return cached
   data. This ensures it returns promptly, and doesn't hang waiting for a
   response when the node is having issues. */
void status(CryptoNote::INode &node, CryptoNote::WalletGreen &wallet)
{
    uint32_t localHeight = node.getLastLocalBlockHeight();
    uint32_t remoteHeight = node.getLastKnownBlockHeight();
    uint32_t walletHeight = wallet.getBlockCount();

    /* Print the heights of local, remote, and wallet */
    printHeights(localHeight, remoteHeight, walletHeight);

    std::cout << std::endl;

    /* Print the network and wallet sync status in percentage */
    printSyncStatus(localHeight, remoteHeight, walletHeight);

    std::cout << std::endl;

    /* Print the network hashrate, based on the last local block */
    printHashrate(node.getLastLocalBlockHeaderInfo().difficulty);

    /* Print the amount of peers we have */
    printPeerCount(node.getPeerCount());

    std::cout << std::endl;

    /* Print a summary of the sync status */
    printSyncSummary(localHeight, remoteHeight, walletHeight);
}

void reset(CryptoNote::INode &node, std::shared_ptr<WalletInfo> walletInfo)
{
    uint64_t scanHeight = getScanHeight();

    std::cout << std::endl
              << InformationMsg("This process may take some time to complete.")
              << std::endl
              << InformationMsg("You can't make any transactions during the ")
              << InformationMsg("process.")
              << std::endl << std::endl;
    
    if (!confirm("Are you sure?"))
    {
        return;
    }
    
    std::cout << InformationMsg("Resetting wallet...") << std::endl;

    walletInfo->wallet.reset(scanHeight);

    syncWallet(node, walletInfo);
}

void saveCSV(CryptoNote::WalletGreen &wallet, CryptoNote::INode &node)
{
    const size_t numTransactions = wallet.getTransactionCount();

    std::ofstream csv;
    csv.open(WalletConfig::csvFilename);

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

    /* Loop through transactions */
    for (size_t i = 0; i < numTransactions; i++)
    {
        const CryptoNote::WalletTransaction t = wallet.getTransaction(i);

        /* Ignore fusion transactions */
        if (t.totalAmount == 0)
        {
            continue;
        }

        const std::string amount = formatAmountBasic(std::abs(t.totalAmount));

        const std::string direction = t.totalAmount > 0 ? "IN" : "OUT";

        csv << unixTimeToDate(t.timestamp) << ","       /* Timestamp */
            << t.blockHeight << ","                     /* Block Height */
            << Common::podToHex(t.hash) << ","          /* Hash */
            << amount << ","                            /* Amount */
            << direction                                /* In/Out */
            << std::endl;
    }

    csv.close();

    std::cout << SuccessMsg("CSV successfully written to ")
              << SuccessMsg(WalletConfig::csvFilename)
              << SuccessMsg("!")
              << std::endl;
}

void printOutgoingTransfer(CryptoNote::WalletTransaction t,
                           CryptoNote::INode &node)
{
    std::cout << WarningMsg("Outgoing transfer:")
              << std::endl
              << WarningMsg("Hash: " + Common::podToHex(t.hash))
              << std::endl;

    if (t.timestamp != 0)
    {
        std::cout << WarningMsg("Block height: ")
                  << WarningMsg(std::to_string(t.blockHeight))
                  << std::endl
                  << WarningMsg("Timestamp: ")
                  << WarningMsg(unixTimeToDate(t.timestamp))
                  << std::endl;
    }

    std::cout << WarningMsg("Spent: " + formatAmount(-t.totalAmount - t.fee))
              << std::endl
              << WarningMsg("Fee: " + formatAmount(t.fee))
              << std::endl
              << WarningMsg("Total Spent: " + formatAmount(-t.totalAmount))
              << std::endl;

    const std::string paymentID = getPaymentIDFromExtra(t.extra);

    if (paymentID != "")
    {
        std::cout << WarningMsg("Payment ID: " + paymentID) << std::endl;
    }

    std::cout << std::endl;
}

void printIncomingTransfer(CryptoNote::WalletTransaction t,
                           CryptoNote::INode &node)
{
    std::cout << SuccessMsg("Incoming transfer:")
              << std::endl
              << SuccessMsg("Hash: " + Common::podToHex(t.hash))
              << std::endl;

    if (t.timestamp != 0)
    {
        std::cout << SuccessMsg("Block height: ")
                  << SuccessMsg(std::to_string(t.blockHeight))
                  << std::endl
                  << SuccessMsg("Timestamp: ")
                  << SuccessMsg(unixTimeToDate(t.timestamp))
                  << std::endl;
    }


    std::cout << SuccessMsg("Amount: " + formatAmount(t.totalAmount))
              << std::endl;

    const std::string paymentID = getPaymentIDFromExtra(t.extra);

    if (paymentID != "")
    {
        std::cout << SuccessMsg("Payment ID: " + paymentID) << std::endl;
    }

    std::cout << std::endl;
}

void listTransfers(bool incoming, bool outgoing, 
                   CryptoNote::WalletGreen &wallet, CryptoNote::INode &node)
{
    const size_t numTransactions = wallet.getTransactionCount();

    int64_t totalSpent = 0;
    int64_t totalReceived = 0;

    for (size_t i = 0; i < numTransactions; i++)
    {
        const CryptoNote::WalletTransaction t = wallet.getTransaction(i);

        /* Is a fusion transaction (on a view only wallet). It appears to have
           an incoming amount, because we can't detract the outputs (can't
           decrypt them) */
        if (t.fee == 0 && !t.isBase)
        {
            continue;
        }

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

void save(CryptoNote::WalletGreen &wallet)
{
    std::cout << InformationMsg("Saving.") << std::endl;
    wallet.save();
    std::cout << InformationMsg("Saved.") << std::endl;
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
        trim(address);

        std::cout << std::endl;

        if (parseStandardAddress(address, true))
        {
            break;
        }
    }

    while (true)
    {
        std::cout << InformationMsg("Payment ID: ");

        std::getline(std::cin, paymentID);
        trim(paymentID);

        std::vector<uint8_t> extra;

        std::cout << std::endl;

        if (!CryptoNote::createTxExtraWithPaymentId(paymentID, extra))
        {
            std::cout << WarningMsg("Failed to parse! Payment ID's are 64 "
                                    "character hexadecimal strings.")
                      << std::endl << std::endl;

            continue;
        }

        break;
    }

    std::cout << InformationMsg(createIntegratedAddress(address, paymentID))
              << std::endl;
}

void help(std::shared_ptr<WalletInfo> wallet)
{
    if (wallet->viewWallet)
    {
        printCommands(basicViewWalletCommands());
    }
    else
    {
        printCommands(basicCommands());
    }
}

void advanced(std::shared_ptr<WalletInfo> wallet)
{
    /* We pass the offset of the command to know what index to print for
       command numbers */
    if (wallet->viewWallet)
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
