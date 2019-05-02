// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

/////////////////////////////////
#include <zedwallet++/Transfer.h>
/////////////////////////////////

#include <config/WalletConfig.h>

#include <iostream>

#include <Utilities/ColouredMsg.h>
#include <Utilities/FormatTools.h>
#include <Utilities/Input.h>

#include <zedwallet++/Fusion.h>
#include <zedwallet++/GetInput.h>
#include <zedwallet++/Utilities.h>

namespace
{
    void cancel()
    {
        std::cout << WarningMsg("Cancelling transaction.\n");
    }
}

void transfer(
    const std::shared_ptr<WalletBackend> walletBackend,
    const bool sendAll)
{
    std::cout << InformationMsg("Note: You can type cancel at any time to "
                                "cancel the transaction\n\n");

    const bool integratedAddressesAllowed(true), cancelAllowed(true);
    
    const auto unlockedBalance = walletBackend->getTotalUnlockedBalance();

    if (sendAll && unlockedBalance <= WalletConfig::minimumSend)
    {
        std::stringstream stream;

        stream << "The minimum send allowed is "
               << Utilities::formatAmount(WalletConfig::minimumSend)
               << ", but you have "
               << Utilities::formatAmount(unlockedBalance) << "!\n";

        std::cout << WarningMsg(stream.str());
        
        return cancel();
    }

    std::string address = getAddress(
        "What address do you want to transfer to?: ",
        integratedAddressesAllowed, cancelAllowed
    );

    if (address == "cancel")
    {
        return cancel();
    }

    std::cout << "\n";

    std::string paymentID;

    if (address.length() == WalletConfig::standardAddressLength)
    {
        paymentID = getPaymentID(
            "What payment ID do you want to use?\n"
            "These are usually used for sending to exchanges.",
            cancelAllowed
        );

        if (paymentID == "cancel")
        {
            return cancel();
        }

        std::cout << "\n";
    }

    /* nodeFee will be zero if using a node without a fee, so we can add this
       safely */
    const auto [nodeFee, nodeAddress] = walletBackend->getNodeFee();

    const uint64_t fee = WalletConfig::defaultFee;

    /* Default amount if we're sending everything */
    uint64_t amount = unlockedBalance - nodeFee - fee;

    if (!sendAll)
    {
        bool success;

        std::tie(success, amount) = getAmountToAtomic(
            "How much " + WalletConfig::ticker + " do you want to send?: ",
            cancelAllowed
        );

        std::cout << "\n";

        if (!success)
        {
            return cancel();
        }
    }

    sendTransaction(walletBackend, address, amount, paymentID);
}

void sendTransaction(
    const std::shared_ptr<WalletBackend> walletBackend,
    const std::string address,
    const uint64_t amount,
    const std::string paymentID)
{
    const auto unlockedBalance = walletBackend->getTotalUnlockedBalance();

    /* nodeFee will be zero if using a node without a fee, so we can add this
       safely */
    const auto [nodeFee, nodeAddress] = walletBackend->getNodeFee();

    const uint64_t fee = WalletConfig::defaultFee;

    /* The total balance required with fees added */
    const uint64_t total = amount + nodeFee + fee;

    if (total > unlockedBalance)
    {
        std::cout << WarningMsg("\nYou don't have enough funds to cover "
                                "this transaction!\n\n")
                  << "Funds needed: "
                  << InformationMsg(Utilities::formatAmount(amount + fee + nodeFee))
                  << " (Includes a network fee of "
                  << InformationMsg(Utilities::formatAmount(fee))
                  << " and a node fee of "
                  << InformationMsg(Utilities::formatAmount(nodeFee))
                  << ")\nFunds available: "
                  << SuccessMsg(Utilities::formatAmount(unlockedBalance)) << "\n\n";

        return cancel();
    }

    if (!confirmTransaction(walletBackend, address, amount, paymentID, nodeFee))
    {
        return cancel();
    }

    Error error;

    Crypto::Hash hash;

    std::tie(error, hash) = walletBackend->sendTransactionBasic(
        address, amount, paymentID
    );

    if (error == TOO_MANY_INPUTS_TO_FIT_IN_BLOCK)
    {
        std::cout << WarningMsg("Your transaction is too large to be accepted "
                                "by the network!\n")
                  << InformationMsg("We're attempting to optimize your wallet,\n"
                                    "which hopefully make the transaction small "
                                    "enough to fit in a block.\n"
                                    "Please wait, this will take some time...\n\n");

        /* Try and perform some fusion transactions to make our inputs bigger */
        optimize(walletBackend);

        /* Resend the transaction */
        std::tie(error, hash) = walletBackend->sendTransactionBasic(
            address, amount, paymentID
        );

        /* Still too big, split it up (with users approval) */
        if (error == TOO_MANY_INPUTS_TO_FIT_IN_BLOCK)
        {
            splitTX(walletBackend, address, amount, paymentID);
            return;
        }
    }

    if (error)
    {
        std::cout << WarningMsg("Failed to send transaction: ")
                  << WarningMsg(error) << std::endl;
    }
    else
    {
        std::cout << SuccessMsg("Transaction has been sent!\nHash: ")
                  << SuccessMsg(hash) << "\n";
    }
}

void splitTX(
    const std::shared_ptr<WalletBackend> walletBackend,
    const std::string address,
    const uint64_t amount,
    const std::string paymentID)
{
    std::cout << InformationMsg("Transaction is still too large to send, splitting into "
                                "multiple chunks.\n\n")
              << WarningMsg("It will slightly raise the fee you have to pay,\n"
                            "and hence reduce the total amount you can send if\n"
                            "your balance cannot cover it.\n\n"
                            "If the node you are using charges a fee,\nyou will "
                            "have to pay this fee for each transction.\n");

    if (!Utilities::confirm("Is this OK?"))
    {
        return cancel();
    }

    uint64_t unlockedBalance = walletBackend->getTotalUnlockedBalance();

    uint64_t totalAmount = amount;
    uint64_t sentAmount = 0;
    uint64_t remainder = totalAmount - sentAmount;

    /* How much to split the remaining balance to be sent into each individual
       transaction. If it's 1, then we'll attempt to send the full amount,
       if it's 2, we'll send half, and so on. */
    uint64_t amountDivider = 1;

    int txNumber = 1;

    const auto [nodeFee, nodeAddress] = walletBackend->getNodeFee();

    while (true)
    {
        uint64_t splitAmount = remainder / amountDivider;

        /* If we have odd numbers, we can have an amount that is smaller
           than the remainder to send, but the remainder is less than
           2 * amount.
           So, we include this amount in our current transaction to prevent
           this change not being sent.
           If we're trying to send more than the remaining amount, set to
           the remaining amount. */
        if (splitAmount != remainder && remainder < (splitAmount * 2))
        {
            splitAmount = remainder;
        }

        uint64_t totalNeeded = splitAmount + WalletConfig::minimumFee + nodeFee;

        /* Don't have enough to cover the full transfer, just send as much
           as we can (deduct fees which will be added later) */
        if (totalNeeded > unlockedBalance)
        {
            totalNeeded = unlockedBalance - WalletConfig::minimumFee - nodeFee;
            splitAmount = totalNeeded - WalletConfig::minimumFee + nodeFee;
        }

        if (splitAmount < WalletConfig::minimumSend)
        {
            std::cout << WarningMsg("Failed to split up transaction, sorry.\n");
            return;
        }

        /* Balance is going to get locked as we send, wait for it to unlock
           and then send */
        while (walletBackend->getTotalUnlockedBalance() < totalNeeded)
        {
            std::cout << WarningMsg("Waiting for balance to unlock to send "
                                    "next transaction.\n"
                                    "Will try again in 15 seconds...\n\n");

            std::this_thread::sleep_for(std::chrono::seconds(15));
        }

        const auto [error, hash] = walletBackend->sendTransactionBasic(
            address, splitAmount, paymentID
        );

        /* Still too big, reduce amount */
        if (error == TOO_MANY_INPUTS_TO_FIT_IN_BLOCK)
        {
            amountDivider *= 2;

            /* This can take quite a long time getting mixins each time
               so let them know it's not frozen */
            std::cout << InformationMsg("Working...\n");

            continue;
        }
        else if (error)
        {
            std::cout << WarningMsg("Failed to send transaction: ")
                      << error << "\nAborting, sorry...";
            return;
        }

        std::stringstream stream;

        stream << "Transaction number " << txNumber << " has been sent!\nHash: "
               << hash << "\nAmount: " << Utilities::formatAmount(splitAmount)
               << "\n\n";

        std::cout << SuccessMsg(stream.str()) << std::endl;

        txNumber++;

        sentAmount += splitAmount;

        /* Remember to remove the fee and node fee as well from balance */
        unlockedBalance -= splitAmount - WalletConfig::minimumFee - nodeFee;

        remainder = totalAmount - sentAmount;

        /* We've sent the full amount required now */
        if (sentAmount == totalAmount)
        {
            std::cout << InformationMsg("All transactions have been sent!\n");
            return;
        }

        /* Went well, revert to original divider */
        amountDivider = 1;
    }
}

bool confirmTransaction(
    const std::shared_ptr<WalletBackend> walletBackend,
    const std::string address,
    const uint64_t amount,
    const std::string paymentID,
    const uint64_t nodeFee)
{
    std::cout << InformationMsg("\nConfirm Transaction?\n");

    std::cout << "You are sending "
              << SuccessMsg(Utilities::formatAmount(amount))
              << ", with a network fee of " 
              << SuccessMsg(Utilities::formatAmount(WalletConfig::defaultFee))
              << ",\nand a node fee of "
              << SuccessMsg(Utilities::formatAmount(nodeFee));

    if (paymentID != "")
    {
        std::cout << ",\nand a Payment ID of " << SuccessMsg(paymentID);
    }
    else
    {
        std::cout << ".";
    }
    
    std::cout << "\n\nFROM: " << SuccessMsg(walletBackend->getWalletLocation())
              << "\nTO: " << SuccessMsg(address) << "\n\n";

    if (Utilities::confirm("Is this correct?"))
    {
        /* Use default message */
        ZedUtilities::confirmPassword(walletBackend, "Confirm your password: ");
        return true;
    }

    return false;
}
