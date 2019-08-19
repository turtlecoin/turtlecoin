// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

/////////////////////////////////
#include <zedwallet++/Commands.h>
/////////////////////////////////

#include <config/WalletConfig.h>
#include <utilities/Container.h>

std::vector<Command> startupCommands()
{
    return {
        Command("open", "Open a wallet already on your system"),
        Command("create", "Create a new wallet"),
        Command("seed_restore", "Restore a wallet using a seed phrase of words"),
        Command("key_restore", "Restore a wallet using a view and spend key"),
        Command("view_wallet", "Import a view only wallet"),
        Command("exit", "Exit the program"),
    };
}

std::vector<Command> nodeDownCommands()
{
    return {
        Command("try_again", "Try to connect to the node again"),
        Command("continue", "Continue to the wallet interface regardless"),
        Command("swap_node", "Specify a new daemon address/port to connect to"),
        Command("exit", "Exit the program"),
    };
}

std::vector<AdvancedCommand> allCommands()
{
    return {
        /* Basic commands */
        AdvancedCommand("advanced", "List available advanced commands", true, false),
        AdvancedCommand("address", "Display your payment address", true, false),
        AdvancedCommand("balance", "Display how much " + WalletConfig::ticker + " you have", true, false),
        AdvancedCommand("backup", "Backup your private keys and/or seed", true, false),
        AdvancedCommand("exit", "Exit and save your wallet", true, false),
        AdvancedCommand("help", "List this help message", true, false),
        AdvancedCommand("transfer", "Send " + WalletConfig::ticker + " to someone", false, false),

        /* Advanced commands */
        AdvancedCommand("ab_add", "Add a person to your address book", true, true),
        AdvancedCommand("ab_delete", "Delete a person in your address book", true, true),
        AdvancedCommand("ab_list", "List everyone in your address book", true, true),
        AdvancedCommand("ab_send", "Send " + WalletConfig::ticker + " to someone in your address book", false, true),
        AdvancedCommand("change_password", "Change your wallet password", true, true),
        AdvancedCommand("get_tx_private_key", "Get the private key of a transaction", true, true),
        AdvancedCommand("make_integrated_address", "Make a combined address + payment ID", true, true),
        AdvancedCommand("incoming_transfers", "Show incoming transfers", true, true),
        AdvancedCommand("list_transfers", "Show all transfers", false, true),
        AdvancedCommand("optimize", "Optimize your wallet to send large amounts", false, true),
        AdvancedCommand("outgoing_transfers", "Show outgoing transfers", false, true),
        AdvancedCommand("reset", "Recheck the chain from zero for transactions", true, true),
        AdvancedCommand("save", "Save your wallet state", true, true),
        AdvancedCommand("save_csv", "Save all wallet transactions to a CSV file", true, true),
        AdvancedCommand("send_all", "Send all your balance to someone", false, true),
        AdvancedCommand("set_log_level", "Alter the logging level", true, true),
        AdvancedCommand("status", "Display sync status and network hashrate", true, true),
        AdvancedCommand("swap_node", "Specify a new daemon address/port to sync from", true, true),
    };
}

std::vector<AdvancedCommand> basicCommands()
{
    return Utilities::filter(allCommands(), [](AdvancedCommand c) { return !c.advanced; });
}

std::vector<AdvancedCommand> advancedCommands()
{
    return Utilities::filter(allCommands(), [](AdvancedCommand c) { return c.advanced; });
}

std::vector<AdvancedCommand> basicViewWalletCommands()
{
    return Utilities::filter(basicCommands(), [](AdvancedCommand c) { return c.viewWalletSupport; });
}

std::vector<AdvancedCommand> advancedViewWalletCommands()
{
    return Utilities::filter(advancedCommands(), [](AdvancedCommand c) { return c.viewWalletSupport; });
}

std::vector<AdvancedCommand> allViewWalletCommands()
{
    return Utilities::filter(allCommands(), [](AdvancedCommand c) { return c.viewWalletSupport; });
}
