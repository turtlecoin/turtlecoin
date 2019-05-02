// Copyright (c) 2018-2019, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

/////////////////////////////
#include <zedwallet++/Menu.h>
/////////////////////////////

#include <config/WalletConfig.h>

#include <Utilities/FormatTools.h>

#include <zedwallet++/CommandDispatcher.h>
#include <zedwallet++/Commands.h>
#include <zedwallet++/GetInput.h>

std::tuple<bool, bool, std::shared_ptr<WalletBackend>> selectionScreen(const Config &config)
{
    while (true)
    {
        /* Get the users action */
        std::string launchCommand = getAction(config);

        /* User wants to exit */
        if (launchCommand == "exit")
        {
            const bool exit(true), sync(false);

            return {exit, sync, nullptr};
        }

        /* Handle the user input */
        std::shared_ptr<WalletBackend> walletBackend = handleLaunchCommand(
            launchCommand, config
        );

        /* Action failed, for example wallet file is corrupted. */
        if (walletBackend == nullptr)
        {
            std::cout << InformationMsg("Returning to selection screen...")
                      << std::endl;

            continue;
        }

        /* Node is down, user wants to exit */
        if (!checkNodeStatus(walletBackend))
        {
            const bool exit(true), sync(false);

            return {exit, sync, nullptr};
        }

        const auto [feeAmount, feeAddress] = walletBackend->getNodeFee();

        if (feeAmount != 0)
        {
            std::stringstream feemsg;

            feemsg << "You have connected to a node that charges "
                      "a fee to send transactions.\n\n"
                      "The fee for sending transactions is: "
                   << Utilities::formatAmount(feeAmount)
                   << " per transaction.\n\n"
                      "If you don't want to pay the node fee, please "
                      "relaunch "
                   << WalletConfig::walletName
                   << " and specify a different node or run your own.";

            std::cout << WarningMsg(feemsg.str()) << std::endl;
        }

        /* If we're creating a wallet, don't print the lengthy sync process */
        if (launchCommand == "create")
        {
            std::stringstream str;

            str << std::endl
                << "Your wallet is syncing with the network in the background."
                << std::endl
                << "Until this is completed new transactions might not show "
                << "up." << std::endl
                << "Use the status command to check the progress."
                << std::endl;

            std::cout << InformationMsg(str.str());

            const bool exit(false), sync(false);

            return {exit, sync, walletBackend};
        }

        const bool exit(false), sync(true);
    
        /* Return the wallet info */
        return {exit, sync, walletBackend};
    }
}

bool checkNodeStatus(const std::shared_ptr<WalletBackend> walletBackend)
{
    while (true)
    {
        if (walletBackend->daemonOnline())
        {
            break;
        }

        std::stringstream msg;

        msg << "It looks like " << WalletConfig::daemonName << " isn't open!\n\n"
            << "Ensure " << WalletConfig::daemonName
            << " is open and has finished syncing. "
            << "(It will often not respond when syncing)\n"
            << "If it's still not working, try restarting "
            << WalletConfig::daemonName << " (or try a different remote node)."
            << "\nThe daemon sometimes gets stuck.\nAlternatively, perhaps "
            << WalletConfig::daemonName << " can't communicate with any peers."
            << "\n\nThe wallet can't function fully until it can communicate with "
            << "the network.";

        std::cout << WarningMsg(msg.str()) << std::endl;

        /* Print the commands */
        printCommands(nodeDownCommands());

        /* See what the user wants to do */
        std::string command = parseCommand(
            nodeDownCommands(), nodeDownCommands(),
            "What would you like to do?: "
        );

        /* If they want to try again, check the node height again */
        if (command == "try_again")
        {
            continue;
        }
        /* If they want to exit, exit */
        else if (command == "exit")
        {
            return false;
        }
        /* If they want to continue, proceed to the menu screen */
        else if (command == "continue")
        {
            return true;
        }
        /* User wants to try a different node */
        else if (command == "swap_node")
        {
            const auto [host, port, ssl] = getDaemonAddress();

            std::cout << InformationMsg("\nSwapping node, this may take some time...\n");

            walletBackend->swapNode(host, port, ssl);

            std::cout << SuccessMsg("Node swap complete.\n\n");

            continue;
        }
    }

    return true;
}

std::string getAction(const Config &config)
{
    if (config.walletGiven || config.passGiven)
    {
        return "open";
    }

    printCommands(startupCommands());

    return parseCommand(
        startupCommands(), startupCommands(), "What would you like to do?: "
    );
}

void mainLoop(
    const std::shared_ptr<WalletBackend> walletBackend,
    const std::shared_ptr<std::mutex> mutex)
{
    if (walletBackend->isViewWallet())
    {
        printCommands(basicViewWalletCommands());
    }
    else
    {
        printCommands(basicCommands());
    }
    
    while (true)
    {
        std::string command;

        if (walletBackend->isViewWallet())
        {
            command = parseCommand(
                basicViewWalletCommands(), allViewWalletCommands(),
                getPrompt(walletBackend)
            );
        }
        else
        {
            command = parseCommand(
                basicCommands(), allCommands(), getPrompt(walletBackend)
            );
        }

        /* User exited */
        if (!handleCommand(command, walletBackend, mutex))
        {
            return;
        }
    }
}
