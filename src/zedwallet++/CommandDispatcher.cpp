// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

//////////////////////////////////////////
#include <zedwallet++/CommandDispatcher.h>
//////////////////////////////////////////

#include <iostream>

#include <Utilities/ColouredMsg.h>
#include <Utilities/Input.h>

#include <zedwallet++/AddressBook.h>
#include <zedwallet++/CommandImplementations.h>
#include <zedwallet++/Open.h>
#include <zedwallet++/Transfer.h>
#include <zedwallet++/Fusion.h>
#include <zedwallet++/Utilities.h>

bool handleCommand(
    const std::string command,
    const std::shared_ptr<WalletBackend> walletBackend,
    const std::shared_ptr<std::mutex> mutex)
{
    /* Aquire the lock so transactions don't get printed whilst we're handling
       a command */
    std::scoped_lock lock(*mutex);

    /* Basic commands */
    if (command == "advanced")
    {
        advanced(walletBackend);
    }
    else if (command == "address")
    {
        std::cout << SuccessMsg(walletBackend->getPrimaryAddress()) << std::endl;
    }
    else if (command == "balance")
    {
        balance(walletBackend);
    }
    else if (command == "backup")
    {
        backup(walletBackend);
    }
    else if (command == "exit")
    {
        return false;
    }
    else if (command == "help")
    {
        help(walletBackend);
    }
    else if (command == "transfer")
    {
        const bool sendAll = false;

        transfer(walletBackend, sendAll);
    }
    /* Advanced commands */
    else if (command == "ab_add")
    {
        addToAddressBook();
    }
    else if (command == "ab_delete")
    {
        deleteFromAddressBook();
    }
    else if (command == "ab_list")
    {
        listAddressBook();
    }
    else if (command == "ab_send")
    {
        sendFromAddressBook(walletBackend);
    }
    else if (command == "change_password")
    {
        changePassword(walletBackend);
    }
    else if (command == "get_tx_private_key")
    {
        getTxPrivateKey(walletBackend);
    }
    else if (command == "make_integrated_address")
    {
        createIntegratedAddress();
    }
    else if (command == "incoming_transfers")
    {
        const bool printIncoming = true;
        const bool printOutgoing = false;

        listTransfers(printIncoming, printOutgoing, walletBackend);
    }
    else if (command == "list_transfers")
    {
        const bool printIncoming = true;
        const bool printOutgoing = true;

        listTransfers(printIncoming, printOutgoing, walletBackend);
    }
    else if (command == "optimize")
    {
        std::cout << "Attempting to optimize your wallet to allow you to "
                     "send large amounts at once.\n"
                  << WarningMsg("This may take a very long time!\n");

        if (!Utilities::confirm("Do you want to proceed?"))
        {
            std::cout << WarningMsg("Cancelling optimization.") << std::endl;
        }
        else
        {
            optimize(walletBackend);
        }
    }
    else if (command == "outgoing_transfers")
    {
        const bool printIncoming = false;
        const bool printOutgoing = true;

        listTransfers(printIncoming, printOutgoing, walletBackend);
    }
    else if (command == "reset")
    {
        reset(walletBackend);
    }
    else if (command == "save")
    {
        save(walletBackend);
    }
    else if (command == "save_csv")
    {
        saveCSV(walletBackend);
    }
    else if (command == "send_all")
    {
        const bool sendAll = true;

        transfer(walletBackend, sendAll);
    }
    else if (command == "status")
    {
        status(walletBackend);
    }
    else if (command == "swap_node")
    {
        swapNode(walletBackend);
    }
    /* This should never happen */
    else
    {
        throw std::runtime_error("Command was defined but not hooked up!");
    }

    return true;
}

std::shared_ptr<WalletBackend> handleLaunchCommand(
    const std::string launchCommand,
    const Config &config)
{
    if (launchCommand == "create")
    {
        return createWallet(config);
    }
    else if (launchCommand == "open")
    {
        return openWallet(config);
    }
    else if (launchCommand == "seed_restore")
    {
        return importWalletFromSeed(config);
    }
    else if (launchCommand == "key_restore")
    {
        return importWalletFromKeys(config);
    }
    else if (launchCommand == "view_wallet")
    {
        return importViewWallet(config);
    }
    /* This should never happen */
    else
    {
        throw std::runtime_error("Command was defined but not hooked up!");
    }
}
