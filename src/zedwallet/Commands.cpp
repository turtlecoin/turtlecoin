// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

///////////////////////////////
#include <zedwallet/Commands.h>
///////////////////////////////

#include <zedwallet/AddressBook.h>
#include <zedwallet/ColouredMsg.h>
#include <zedwallet/CommandImplementations.h>
#include <zedwallet/Transfer.h>
#include <zedwallet/Fusion.h>
#include <zedwallet/WalletConfig.h>

const Maybe<Command> contains(std::string name,
                                    std::vector<Command> &commands)
{
    for (auto command : commands)
    {
        if (command.name == name)
        {
            return Just<Command>(command);
        }
    }

    return Nothing<Command>();
}

const std::vector<Command> filterCommands(std::vector<Command> &commands,
                                    std::function<bool(Command)> predicate)
{
    std::vector<Command> result;

    std::copy_if(commands.begin(), commands.end(),
                 std::back_inserter(result), predicate);

    return result;
}

bool dispatchCommand(std::shared_ptr<WalletInfo> &walletInfo,
                     CryptoNote::INode &node, std::string command)
{
    auto commands = allCommands();
    auto available = availableCommands(walletInfo->viewWallet, commands);
    auto maybeCommand = resolveCommand(command, commands, available);

    if (!maybeCommand.isJust)
    {
        return false;
    }

    /* If the user inputted a numeric command, convert it back to the actual
       command name */
    command = maybeCommand.x.name;

    if (walletInfo->viewWallet && !maybeCommand.x.viewWalletSupport)
    {
        /* Command exists, but user has a view wallet and this command cannot
           be used in a view wallet */
        std::cout << WarningMsg("This command is not available in a "
                                "view only wallet...") << std::endl;

        return false;
    }

    /* Can't use a switch with std::string ;((( */
    if (command == "export_keys")
    {
        exportKeys(walletInfo);
    }
    else if (command == "help")
    {
        listCommands(available, false);
    }
    else if (command == "advanced")
    {
        listCommands(available, true);
    }
    else if (command == "balance")
    {
        balance(node, walletInfo->wallet, walletInfo->viewWallet);
    }
    else if (command == "address")
    {
        std::cout << SuccessMsg(walletInfo->walletAddress) << std::endl;
    }
    else if (command == "incoming_transfers")
    {
        listTransfers(true, false, walletInfo->wallet, node);
    }
    else if (command == "save_csv")
    {
        saveCSV(walletInfo->wallet, node);
    }
    else if (command == "exit")
    {
        return true;
    }
    else if (command == "save")
    {
        save(walletInfo->wallet);
    }
    else if (command == "status")
    {
        status(node, walletInfo->wallet);
    }
    else if (command == "reset")
    {
        reset(node, walletInfo);
    }
    else if (command == "outgoing_transfers")
    {
        listTransfers(false, true, walletInfo->wallet, node);
    }
    else if (command == "list_transfers")
    {
        listTransfers(true, true, walletInfo->wallet, node);
    }
    else if (command == "transfer")
    {
        transfer(walletInfo, node.getLastKnownBlockHeight(), false, node.feeAddress(), node.feeAmount());
    }
    else if (command == "optimize")
    {
        fullOptimize(walletInfo->wallet);
    }
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
        sendFromAddressBook(walletInfo, node.getLastKnownBlockHeight(), node.feeAddress(), node.feeAmount());
    }
    else if (command == "change_password")
    {
        changePassword(walletInfo);
    }
    else if (command == "make_integrated_address")
    {
        createIntegratedAddress();
    }
    else if (command == "send_all")
    {
        transfer(walletInfo, node.getLastKnownBlockHeight(), true, node.feeAddress(), node.feeAmount());
    }
    /* This should never happen */
    else
    {
        std::cout << WarningMsg("Command was defined but not hooked up: ")
                  << InformationMsg(command)
                  << std::endl
                  << InformationMsg("Please report this bug!")
                  << std::endl;
    }

    return false;
}

const Maybe<Command> resolveCommand(std::string command,
                                    std::vector<Command> &allCommands,
                                    std::vector<Command> &available)
{
    int index;

    /* See if the value is a numberic choice rather than a string command */
    try
    {
        /* Standard adding one to inputs to be more user friendly, 1 based
           indexing */
        index = std::stoi(command);
        index--;

        if (index < 0 || index >= static_cast<int>(available.size()))
        {
            std::cout << WarningMsg("Bad input: Expected a command name, "
                                    "or number from ")
                      << InformationMsg("1")
                      << WarningMsg(" to ")
                      << InformationMsg(std::to_string(available.size()))
                      << std::endl;

            return Nothing<Command>();
        }

        command = available[index].name;
    }
    catch (const std::invalid_argument &)
    {
        /* not a number */
    }

    const auto maybeCommand = contains(command, allCommands);

    if (command == "")
    {
        return Nothing<Command>();
    }

    /* Command doesn't exist */
    if (!maybeCommand.isJust)
    {
        std::cout << "Unknown command: " << WarningMsg(command) 
                  << ", use " << SuggestionMsg("help")
                  << " command to list all possible commands."
                  << std::endl;

        return Nothing<Command>();
    }

    return maybeCommand;
}

std::vector<Command> allCommands()
{
    /* Add things to this in alphabetical order so it's nicer to read please
       :) */
    std::vector<Command> commands =
    {
        /* Basic commands */
        {"address", "Display your payment address", true, false},
        {"advanced", "List available advanced commands", true, false},

        {"balance", "Display how much " + WalletConfig::ticker + 
                    " you have", true, false},

        {"exit", "Exit and save your wallet", true, false},
        {"export_keys", "Export your private keys", true, false},
        {"help", "List this help message", true, false},

        {"transfer", "Send " + WalletConfig::ticker +
                     " to someone", false, false},

        /* Advanced commands */
        {"ab_add", "Add a person to your address book", true, true},
        {"ab_delete", "Delete a person from your address book", true, true},
        {"ab_list", "List everyone in your address book", true, true},

        {"ab_send", "Send " + WalletConfig::ticker + 
                    " to someone in your address book", false, true},

        {"change_password", "Change your wallet password", true, true},

        {"make_integrated_address", "Make an integrated address from an "
                                    "address + payment ID", true, true},

        {"incoming_transfers", "Show incoming transfers", true, true},
        {"list_transfers", "Show all transfers", false, true},
        {"optimize", "Optimize your wallet to send large amounts", false, true},
        {"outgoing_transfers", "Show outgoing transfers", false, true},
        {"reset", "Recheck the chain from zero for transactions", true, true},
        {"save", "Save your wallet state", true, true},
        {"save_csv", "Save all wallet transactions to a CSV file", true, true},
        {"send_all", "Send all your balance to someone", false, true},
        {"status", "Display sync status and network hashrate", true, true}
    };

    /* Pop em in alphabetical order */
    std::sort(commands.begin(), commands.end(), [](const Command &lhs,
                                                   const Command &rhs)
    {
        /* If both are the same command type (basic or advanced), compare
           based on name. */
        if (lhs.advanced == rhs.advanced)
        {
            return lhs.name < rhs.name;
        }

        return lhs.advanced < rhs.advanced;
    });

    return commands;
}

/* The commands which are currently usable */
const std::vector<Command> availableCommands(bool viewWallet,
                                                   std::vector<Command>
                                                   &commands)
{
    if (!viewWallet)
    {
        return commands;
    }

    return filterCommands(commands, [](Command c)
    {
        return c.viewWalletSupport;
    });
}

uint64_t numBasicCommands(std::vector<Command> &commands)
{
    return std::count_if(commands.begin(), commands.end(), [](Command c)
    {
        return !c.advanced;
    });
}

void listCommands(std::vector<Command> &commands, bool advanced)
{
    const int commandPadding = 25;

    uint64_t index = 1;

    if (advanced)
    {
        /* We want the basic commands to be the first numbers, then after
           that, list the advanced commands numbers */
        index = numBasicCommands(commands) + 1;
    }

    for (const auto command : commands)
    {
        if (command.advanced == advanced)
        {
            std::cout << " " << InformationMsg(std::to_string(index)) << "\t"
                      << SuccessMsg(command.name, commandPadding)
                      << command.description << std::endl;

            index++;
        }
    }
}
