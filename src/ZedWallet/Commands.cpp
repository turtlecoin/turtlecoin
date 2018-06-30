// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

//////////////////////////////////
#include <ZedWallet/Commands.h>
//////////////////////////////////

#include <Common/StringTools.h>

#include <CryptoNoteCore/Account.h>

#include <memory>

#include <Mnemonics/electrum-words.h>

#include <ZedWallet/ColouredMsg.h>
#include <ZedWallet/Fusion.h>
#include <ZedWallet/Sync.h>
#include <ZedWallet/Tools.h>
#include <ZedWallet/Transfer.h>
#include <ZedWallet/Types.h>

void exportKeys(std::shared_ptr<WalletInfo> &walletInfo)
{
    confirmPassword(walletInfo->walletPass);
    printPrivateKeys(walletInfo->wallet, walletInfo->viewWallet);
}

void printPrivateKeys(CryptoNote::WalletGreen &wallet, bool viewWallet)
{
    Crypto::SecretKey privateViewKey = wallet.getViewKey().secretKey;

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

    CryptoNote::AccountBase::generateViewFromSpend(privateSpendKey,
                                                   derivedPrivateViewKey);

    bool deterministicPrivateKeys = derivedPrivateViewKey == privateViewKey;

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
        std::string mnemonicSeed;

        crypto::ElectrumWords::bytes_to_words(privateSpendKey,
                                              mnemonicSeed,
                                              "English");

        std::cout << std::endl
                  << SuccessMsg("Mnemonic seed:")
                  << std::endl
                  << SuccessMsg(mnemonicSeed)
                  << std::endl;
    }
}

void help(bool viewWallet)
{
    std::cout << "Available commands:" << std::endl
              << SuccessMsg("help", 25)
              << "List this help message" << std::endl
              << SuccessMsg("reset", 25)
              << "Discard cached data and recheck for transactions" << std::endl
              << SuccessMsg("bc_height", 25)
              << "Show the blockchain height" << std::endl
              << SuccessMsg("balance", 25)
              << "Display how much TRTL you have" << std::endl
              << SuccessMsg("export_keys", 25)
              << "Export your private keys" << std::endl
              << SuccessMsg("address", 25)
              << "Display your payment address" << std::endl
              << SuccessMsg("exit", 25)
              << "Exit and save your wallet" << std::endl
              << SuccessMsg("save", 25)
              << "Save your wallet state" << std::endl
              << SuccessMsg("status", 25)
              << "Show daemon status" << std::endl
              << SuccessMsg("incoming_transfers", 25)
              << "Show incoming transfers" << std::endl;

    if (viewWallet)
    {
        std::cout << InformationMsg("Please note you are using a view only "
                                    "wallet, and so cannot transfer TRTL.")
                  << std::endl;
    } else
    {
        std::cout << SuccessMsg("outgoing_transfers", 25)
                  << "Show outgoing transfers" << std::endl
                  << SuccessMsg("list_transfers", 25)
                  << "Show all transfers" << std::endl
                  << SuccessMsg("save_csv", 25)
                  << "Save all wallet transactions to CSV file" << std::endl
                  << SuccessMsg("optimize", 25)
                  << "Optimize your wallet to send large amounts"
                  << std::endl
                  << SuccessMsg("transfer", 25)
                  << "Send TRTL to someone" << std::endl;
    }
}

void status(CryptoNote::INode &node)
{
    std::cout << node.getInfo() << std::endl;
}

void balance(CryptoNote::INode &node, CryptoNote::WalletGreen &wallet,
             bool viewWallet)
{
    uint64_t unconfirmedBalance = wallet.getPendingBalance();
    uint64_t confirmedBalance = wallet.getActualBalance();
    uint64_t totalBalance = unconfirmedBalance + confirmedBalance;

    uint32_t localHeight = node.getLastLocalBlockHeight();
    uint32_t remoteHeight = node.getLastKnownBlockHeight();
    uint32_t walletHeight = wallet.getBlockCount();

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
                                    "can only track incoming transactions, "
                                    "and so your wallet balance may appear "
                                    "inflated.") << std::endl;
    }

    if (localHeight < remoteHeight)
    {
        std::cout << std::endl
                  << InformationMsg("Your daemon is not fully synced with "
                                    "the network!")
                  << std::endl << "Your balance may be incorrect until you "
                  << "are fully synced!" << std::endl;
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

void blockchainHeight(CryptoNote::INode &node, CryptoNote::WalletGreen &wallet)
{
    uint32_t localHeight = node.getLastLocalBlockHeight();
    uint32_t remoteHeight = node.getLastKnownBlockHeight();
    uint32_t walletHeight = wallet.getBlockCount();

    /* This is the height that the wallet has been scanned to. The blockchain
       can be fully updated, but we have to walk the chain to find our
       transactions, and this number indicates that progress. */
    std::cout << "Wallet blockchain height: ";

    /* Small buffer because wallet height doesn't update instantly like node
       height does */
    if (walletHeight + 1000 > remoteHeight)
    {
        std::cout << SuccessMsg(std::to_string(walletHeight));
    } else
    {
        std::cout << WarningMsg(std::to_string(walletHeight));
    }

    std::cout << std::endl << "Local blockchain height: ";

    if (localHeight == remoteHeight)
    {
        std::cout << SuccessMsg(std::to_string(localHeight));
    } else
    {
        std::cout << WarningMsg(std::to_string(localHeight));
    }

    std::cout << std::endl << "Network blockchain height: "
              << SuccessMsg(std::to_string(remoteHeight)) << std::endl;

    if (localHeight == 0 && remoteHeight == 0)
    {
        std::cout << WarningMsg("Uh oh, it looks like you don't have "
                                "TurtleCoind open!")
                  << std::endl;
    } else if (walletHeight + 1000 < remoteHeight && localHeight == remoteHeight)
    {
        std::cout << InformationMsg("You are synced with the network, but the "
                                    "blockchain is still being scanned for "
                                    "your transactions.")
                  << std::endl
                  << "Balances might be incorrect whilst this is ongoing."
                  << std::endl;
    } else if (localHeight == remoteHeight)
    {
        std::cout << SuccessMsg("Yay! You are synced!") << std::endl;
    } else
    {
        std::cout << WarningMsg("Be patient, you are still syncing with the "
                                "network!") << std::endl;
    }
}

void reset(CryptoNote::INode &node, std::shared_ptr<WalletInfo> &walletInfo)
{
    std::cout << InformationMsg("Resetting wallet...") << std::endl;

    walletInfo->knownTransactionCount = 0;

    /* Wallet is now unitialized. You must reinit with load, initWithKeys,
       or whatever. This function wipes the cache, then saves the wallet. */
    walletInfo->wallet.clearCacheAndShutdown();

    /* Now, we reopen the wallet. It now has no cached tx's, and balance */
    walletInfo->wallet.load(walletInfo->walletFileName,
                            walletInfo->walletPass);

    /* Now we rescan the chain to re-discover our balance and transactions */
    syncWallet(node, walletInfo);
}
