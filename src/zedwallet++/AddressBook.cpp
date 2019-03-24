// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

////////////////////////////////////
#include <zedwallet++/AddressBook.h>
////////////////////////////////////

#include <config/WalletConfig.h>

#include <Errors/ValidateParameters.h>

#include <fstream>

#include <iostream>

#include <Utilities/ColouredMsg.h>
#include <zedwallet++/GetInput.h>
#include <zedwallet++/Transfer.h>
#include <zedwallet++/Utilities.h>

#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

const std::string getAddressBookName(const std::vector<AddressBookEntry> addressBook)
{
    while (true)
    {
        std::string friendlyName;

        std::cout << InformationMsg("What friendly name do you want to ")
                  << InformationMsg("give this address book entry?: ");

        std::getline(std::cin, friendlyName);

        Common::trim(friendlyName);

        const auto it = std::find(addressBook.begin(), addressBook.end(),
                                  AddressBookEntry(friendlyName));

        if (it != addressBook.end())
        {
            std::cout << WarningMsg("An address book entry with this ")
                      << WarningMsg("name already exists!")
                      << std::endl << std::endl;

            continue;
        }

        return friendlyName;
    }
}

void addToAddressBook()
{
    std::cout << InformationMsg("Note: You can type cancel at any time to "
                                "cancel adding someone to your address book.")
              << std::endl << std::endl;

    auto addressBook = getAddressBook();

    const std::string friendlyName = getAddressBookName(addressBook);

    if (friendlyName == "cancel")
    {
        std::cout << WarningMsg("Cancelling addition to address book.")
                  << std::endl;
        return;
    }

    const bool integratedAddressesAllowed(true), cancelAllowed(true);

    const std::string address = getAddress(
        "\nWhat address does this user have?: ", integratedAddressesAllowed,
        cancelAllowed
    );

    if (address == "cancel")
    {
        std::cout << WarningMsg("Cancelling addition to address book.")
                  << std::endl;
        return;
    }

    std::string paymentID;

    /* Don't prompt for a payment ID if we have an integrated address */
    if (address.length() == WalletConfig::standardAddressLength)
    {
        const bool cancelAllowed = true;

        paymentID = getPaymentID(
            "\nDoes this address book entry have a payment ID associated "
            "with it?\n", cancelAllowed
        );

        if (paymentID == "cancel")
        {
            std::cout << WarningMsg("Cancelling addition to address book.")
                      << std::endl;

            return;
        }
    }

    addressBook.emplace_back(friendlyName, address, paymentID);

    if (saveAddressBook(addressBook))
    {
        std::cout << SuccessMsg("\nA new entry has been added to your address "
                                "book!\n");
    }
}

const std::tuple<bool, AddressBookEntry> getAddressBookEntry(
    const std::vector<AddressBookEntry> addressBook)
{
    while (true)
    {
        std::string friendlyName;

        std::cout << InformationMsg("Who do you want to send to from your ")
                  << InformationMsg("address book?: ");

        std::getline(std::cin, friendlyName);

        Common::trim(friendlyName);

        /* \n == no-op */
        if (friendlyName == "")
        {
            continue;
        }

        if (friendlyName == "cancel")
        {
            return {true, AddressBookEntry()};
        }

        try
        {
            const int selectionNum = std::stoi(friendlyName) - 1;

            const int numCommands = static_cast<int>(addressBook.size());

            if (selectionNum < 0 || selectionNum >= numCommands)
            {
                std::cout << WarningMsg("Bad input, expected a friendly name, ")
                          << WarningMsg("or number from ")
                          << InformationMsg("1")
                          << WarningMsg(" to ")
                          << InformationMsg(numCommands)
                          << "\n\n";

                continue;
            }

            return {false, addressBook[selectionNum]};
        }
        catch (const std::out_of_range &)
        {
            const int numCommands = static_cast<int>(addressBook.size());

            std::cout << WarningMsg("Bad input, expected a friendly name, ")
                      << WarningMsg("or number from ")
                      << InformationMsg("1")
                      << WarningMsg(" to ")
                      << InformationMsg(numCommands)
                      << "\n\n";

            continue;
        }
        /* Input isn't a number */
        catch (const std::invalid_argument &)
        {
            const auto it = std::find(addressBook.begin(), addressBook.end(),
                                      AddressBookEntry(friendlyName));

            if (it != addressBook.end())
            {
                return {false, *it};
            }

            std::cout << std::endl
                      << WarningMsg("Could not find a user with the name of ")
                      << InformationMsg(friendlyName)
                      << WarningMsg(" in your address book!")
                      << std::endl << std::endl;
        }

        const bool list = ZedUtilities::confirm(
            "Would you like to list everyone in your address book?"
        );

        std::cout << "\n";

        if (list)
        {
            listAddressBook();
        }
    }
}

void sendFromAddressBook(const std::shared_ptr<WalletBackend> walletBackend)
{
    auto addressBook = getAddressBook();

    if (isAddressBookEmpty(addressBook))
    {
        return;
    }

    std::cout << InformationMsg("Note: You can type cancel at any time to ")
              << InformationMsg("cancel the transaction\n\n");

    const auto [cancel, addressBookEntry] = getAddressBookEntry(addressBook);

    if (cancel)
    {
        std::cout << WarningMsg("Cancelling transaction.\n");
        return;
    }

    const bool cancelAllowed = true;

    const auto [success, amount] = getAmountToAtomic(
        "How much " + WalletConfig::ticker + " do you want to send?: ",
        cancelAllowed
    );

    if (!success)
    {
        std::cout << WarningMsg("Cancelling transaction.\n");
    }

    sendTransaction(
        walletBackend, addressBookEntry.address, amount,
        addressBookEntry.paymentID
    );
}

bool isAddressBookEmpty(const std::vector<AddressBookEntry> addressBook)
{
    if (addressBook.empty())
    {
        std::cout << WarningMsg("Your address book is empty! Add some people ")
                  << WarningMsg("to it first.")
                  << std::endl;

        return true;
    }

    return false;
}

void deleteFromAddressBook()
{
    auto addressBook = getAddressBook();

    if (isAddressBookEmpty(addressBook))
    {
        return;
    }

    while (true)
    {
        std::cout << InformationMsg("Note: You can type cancel at any time ")
                  << InformationMsg("to cancel the deletion.\n\n");

        std::string friendlyName;

        std::cout << InformationMsg("What address book entry do you want to ")
                  << InformationMsg("delete?: ");

        std::getline(std::cin, friendlyName);

        Common::trim(friendlyName);

        if (friendlyName == "cancel")
        {
            std::cout << WarningMsg("Cancelling deletion.\n");
            return;
        }

        const auto it = std::remove(addressBook.begin(), addressBook.end(),
                                    AddressBookEntry(friendlyName));

        if (it != addressBook.end())
        {
            addressBook.erase(it);

            if (saveAddressBook(addressBook))
            {
                std::cout << std::endl
                          << SuccessMsg("This entry has been deleted from ")
                          << SuccessMsg("your address book!")
                          << std::endl;
            }

            return;
        }

        std::cout << WarningMsg("\nCould not find a user with the name of ")
                  << InformationMsg(friendlyName)
                  << WarningMsg(" in your address book!\n\n");

        const bool list = ZedUtilities::confirm(
            "Would you like to list everyone in your address book?"
        );

        std::cout << "\n";

        if (list)
        {
            listAddressBook();
        }
    }
}

void listAddressBook()
{
    const std::vector<AddressBookEntry> addressBook = getAddressBook();

    if (isAddressBookEmpty(addressBook))
    {
        return;
    }

    size_t i = 1;

    for (const auto entry : addressBook)
    {
        std::cout << InformationMsg("Address Book Entry: ")
                  << InformationMsg(i) << InformationMsg(" | ")
                  << SuccessMsg(entry.friendlyName) << "\n"
                  << InformationMsg("Address: ")
                  << SuccessMsg(entry.address) << "\n";

        if (entry.paymentID != "")
        {
            std::cout << InformationMsg("Payment ID: ")
                      << SuccessMsg(entry.paymentID) << "\n\n";
        }
        else
        {
            std::cout << "\n";
        }

        i++;
    }
}

std::vector<AddressBookEntry> getAddressBook()
{
    std::vector<AddressBookEntry> addressBook;
    std::ifstream input(WalletConfig::addressBookFilename);

    if (input) {
        rapidjson::IStreamWrapper isw(input);
        rapidjson::Document d;
        d.ParseStream(isw);
        // d should be an array
        for (rapidjson::Value::ConstValueIterator it = d.Begin(); it != d.End(); ++it) {
			AddressBookEntry addressBookEntry((*it)["friendlyName"].GetString(),
                                              (*it)["address"].GetString()
                                              (*it)["paymentID"].GetString());
			addressBook.push_back(addressBookEntry);
        }
    }

    return addressBook;
}

bool saveAddressBook(const std::vector<AddressBookEntry> addressBook)
{
    rapidjson::StringBuffer string_buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(string_buffer);

    std::ofstream output(WalletConfig::addressBookFilename);

    if (output)
    {
        std::vector<AddressBookEntry>::iterator it;

        writer.StartArray();
        for(it = addressBook.begin(); it != addressBook.end(); it++) {
            writer.StartObject();
            writer.Key("friendlyName");
            writer.String((*it).friendlyName);
            writer.Key("address");
            writer.String((*it).address);
            writer.Key("paymentID");
            writer.String((*it).paymentID);
            writer.EndObject();
        }
        writer.EndArray();
        
        output << std::setw(4) << string_buffer.GetString() << std::endl;
    }
    else
    {
        std::cout << WarningMsg("Failed to save address book to disk!")
                  << std::endl
                  << WarningMsg("Check you are able to write files to your ")
                  << WarningMsg("current directory.") << std::endl;

        return false;
    }

    output.close();

    return true;
}
