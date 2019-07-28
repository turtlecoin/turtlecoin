// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <iostream>
#include <utilities/ColouredMsg.h>
#include <walletbackend/WalletBackend.h>
#include <zedwallet++/ParseArguments.h>

std::tuple<bool, bool, std::shared_ptr<WalletBackend>> selectionScreen(const ZedConfig &config);

bool checkNodeStatus(const std::shared_ptr<WalletBackend> walletBackend);

std::string getAction(const ZedConfig &config);

void mainLoop(const std::shared_ptr<WalletBackend> walletBackend, const std::shared_ptr<std::mutex> mutex);

template<typename T>
std::string parseCommand(
    const std::vector<T> &printableCommands,
    const std::vector<T> &availableCommands,
    const std::string prompt)
{
    while (true)
    {
        std::string selection = getInput(availableCommands, prompt);

        /* Convert to lower case */
        std::transform(selection.begin(), selection.end(), selection.begin(), ::tolower);

        /* \n == no-op */
        if (selection == "")
        {
            continue;
        }

        if (selection == "exit")
        {
            return selection;
        }

        int selectionNum;

        bool isNumericInput;

        try
        {
            /* Input is in 1 based indexing, we need 0 based indexing */
            selectionNum = std::stoi(selection) - 1;
            isNumericInput = true;
        }
        catch (const std::out_of_range &)
        {
            /* Set to minus one so it triggers the selectionNum < 0 check,
               and warns them the input is too large */
            selectionNum = -1;
            isNumericInput = true;
        }
        /* Input ain't a number */
        catch (const std::invalid_argument &)
        {
            isNumericInput = false;
        }

        if (isNumericInput)
        {
            const int numCommands = static_cast<int>(availableCommands.size());

            /* Must be in the bounds of the vector */
            if (selectionNum < 0 || selectionNum >= numCommands)
            {
                std::cout << WarningMsg("Bad input, expected a command name, ") << WarningMsg("or number from ")
                          << InformationMsg("1") << WarningMsg(" to ") << InformationMsg(numCommands) << std::endl;

                /* Print the available commands again if the input is bad */
                printCommands(printableCommands);

                continue;
            }

            return availableCommands[selectionNum].commandName;
        }
        else
        {
            /* Find the command by command name */
            auto it =
                std::find_if(availableCommands.begin(), availableCommands.end(), [&selection](const auto command) {
                    std::string cmd = command.commandName;

                    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

                    return cmd == selection;
                });

            /* Command doesn't exist in availableCommands */
            if (it == availableCommands.end())
            {
                std::cout << "Unknown command: " << WarningMsg(selection) << std::endl;

                /* Print the available commands again if the input is bad */
                printCommands(printableCommands);

                continue;
            }

            return selection;
        }
    }
}

template<typename T> void printCommands(const std::vector<T> &commands, size_t offset = 0)
{
    size_t i = 1 + offset;

    std::cout << "\n";

    for (const auto &command : commands)
    {
        std::cout << InformationMsg(" ") << InformationMsg(i) << "\t"
                  << SuccessMsg(command.commandName, 25) /* Pad to 25 chars */
                  << command.description << std::endl;

        i++;
    }

    std::cout << "\n";
}
