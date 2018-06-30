/*
Copyright (C) 2018, The TurtleCoin developers

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

//////////////////////////////////
#include <ZedWallet/Transfer.h>
//////////////////////////////////

#include <boost/algorithm/string.hpp>

#include <Common/StringTools.h>

#include "CryptoNoteConfig.h"

#include <CryptoNoteCore/CryptoNoteBasicImpl.h>
#include <CryptoNoteCore/TransactionExtra.h>

#include "IWallet.h"

/* NodeErrors.h and WalletErrors.h have some conflicting enums, e.g. they
   both export NOT_INITIALIZED, we can get round this by using a namespace */
namespace NodeErrors
{

#include <NodeRpcProxy/NodeErrors.h>

}

#include <ZedWallet/ColouredMsg.h>
#include <ZedWallet/Fusion.h>
#include <ZedWallet/Tools.h>

namespace WalletErrors
{

#include <Wallet/WalletErrors.h>

}

#include <Wallet/WalletGreen.h>

bool parseAmount(std::string strAmount, uint64_t &amount)
{
    boost::algorithm::trim(strAmount);
    /* If the user entered thousand separators, remove them */
    boost::erase_all(strAmount, ",");

    size_t pointIndex = strAmount.find_first_of('.');
    size_t fractionSize;
    size_t numDecimalPlaces = 2;

    if (std::string::npos != pointIndex)
    {
        fractionSize = strAmount.size() - pointIndex - 1;

        while (numDecimalPlaces < fractionSize && '0' == strAmount.back())
        {
            strAmount.erase(strAmount.size() - 1, 1);
            fractionSize--;
        }

        if (numDecimalPlaces < fractionSize)
        {
            return false;
        }

        strAmount.erase(pointIndex, 1);
    } else
    {
        fractionSize = 0;
    }

    if (strAmount.empty())
    {
        return false;
    }

    if (!std::all_of(strAmount.begin(), strAmount.end(), ::isdigit))
    {
        return false;
    }

    if (fractionSize < numDecimalPlaces)
    {
        strAmount.append(numDecimalPlaces - fractionSize, '0');
    }

    return Common::fromString(strAmount, amount);
}

bool confirmTransaction(CryptoNote::TransactionParameters t,
                        std::shared_ptr<WalletInfo> walletInfo)
{
    std::cout << std::endl
              << InformationMsg("Confirm Transaction?") << std::endl;

    std::cout << "You are sending "
              << SuccessMsg(formatAmount(t.destinations[0].amount))
              << ", with a fee of " << SuccessMsg(formatAmount(t.fee));

    std::string paymentID = getPaymentID(t.extra);

    if (paymentID != "")
    {
        std::cout << ", " << std::endl
                  << "and a Payment ID of " << SuccessMsg(paymentID);
    } else
    {
        std::cout << ".";
    }

    std::cout << std::endl << std::endl
              << "FROM: " << SuccessMsg(walletInfo->walletFileName)
              << std::endl
              << "TO: " << SuccessMsg(t.destinations[0].address)
              << std::endl << std::endl;

    if (confirm("Is this correct?"))
    {
        confirmPassword(walletInfo->walletPass);
        return true;
    }

    return false;
}

void sendMultipleTransactions(CryptoNote::WalletGreen &wallet,
                              std::vector<CryptoNote::TransactionParameters>
                              transfers)
{
    size_t numTxs = transfers.size();
    size_t currentTx = 1;

    std::cout << "Your transaction has been split up into " << numTxs
              << " separate transactions of "
              << formatAmount(transfers[0].destinations[0].amount)
              << ". "
              << std::endl
              << "It may take some time to send all the transactions."
              << std::endl << std::endl;

    for (auto tx : transfers)
    {
        while (true)
        {
            std::cout << "Attempting to send transaction "
                      << InformationMsg(std::to_string(currentTx))
                      << " of " << InformationMsg(std::to_string(numTxs))
                      << std::endl;

            wallet.updateInternalCache();

            uint64_t neededBalance = tx.destinations[0].amount + tx.fee;

            if (neededBalance < wallet.getActualBalance())
            {
                size_t id = wallet.transfer(tx);

                CryptoNote::WalletTransaction sentTx
                        = wallet.getTransaction(id);

                std::cout << SuccessMsg("Transaction has been sent!")
                          << std::endl
                          << SuccessMsg("Hash: "
                                        + Common::podToHex(sentTx.hash))
                          << std::endl << std::endl;

                break;
            }

            std::cout << "Waiting for balance to unlock to send next "
                      << "transaction."
                      << std::endl
                      << "Will try again in 5 seconds..."
                      << std::endl << std::endl;

            std::this_thread::sleep_for(std::chrono::seconds(5));
        }

        currentTx++;
    }

    std::cout << SuccessMsg("All transactions sent!") << std::endl;
}

void splitTx(CryptoNote::WalletGreen &wallet,
             CryptoNote::TransactionParameters p)
{
    std::cout << "Transaction is still too large to send, splitting into "
              << "multiple chunks."
              << std::endl
              << "This may take a long time."
              << std::endl
              << "It may also slightly raise the fee you have to pay,"
              << std::endl
              << "and hence reduce the total amount you can send if"
              << std::endl
              << "your balance cannot cover it." << std::endl;

    if (!confirm("Is this OK?"))
    {
        std::cout << WarningMsg("Cancelling transaction.") << std::endl;
        return;
    }

    CryptoNote::TransactionParameters restoreInitialTx = p;

    uint64_t maxSize = wallet.getMaxTxSize();
    size_t txSize = wallet.getTxSize(p);
    uint64_t minFee = CryptoNote::parameters::MINIMUM_FEE;

    for (int numTxMultiplier = 2;; numTxMultiplier++)
    {
        /* We modify p a bit in this function, so restore back to initial
           state each time */
        p = restoreInitialTx;

        /* We can't just evenly divide a transaction up to be < 115k bytes by
           decreasing the amount we're sending, because depending upon the
           inputs we might need to split into more transactions, so a good
           start is attempting to split them into chunks of 55k bytes or so.
           We then check at the end that each transaction is small enough, and
           if not, we up the numTxMultiplier and try again with more
           transactions. */
        int numTransactions
                = int(numTxMultiplier *
                      (std::ceil(double(txSize) / double(maxSize))));

        /* Split the requested fee over each transaction, i.e. if a fee of 200
           TRTL was requested and we split it into 4 transactions each one will
           have a fee of 5 TRTL. If the fee per transaction is less than the
           min fee, use the min fee. */
        uint64_t feePerTx = std::max(p.fee / numTransactions, minFee);

        uint64_t totalFee = feePerTx * numTransactions;

        uint64_t totalCost = p.destinations[0].amount + totalFee;

        /* If we have to use the minimum fee instead of splitting the total fee,
           then it is possible the user no longer has the balance to cover this
           transaction. So, we slightly lower the amount they are sending. */
        if (totalCost > wallet.getActualBalance())
        {
            p.destinations[0].amount = wallet.getActualBalance() - totalFee;
        }

        uint64_t amountPerTx = p.destinations[0].amount / numTransactions;
        /* Left over amount from integral division */
        uint64_t change = p.destinations[0].amount % numTransactions;

        std::vector<CryptoNote::TransactionParameters> transfers;

        for (int i = 0; i < numTransactions; i++)
        {
            CryptoNote::TransactionParameters tmp = p;
            tmp.destinations[0].amount = amountPerTx;
            tmp.fee = feePerTx;
            transfers.push_back(tmp);
        }

        /* Add the extra change to the first transaction */
        transfers[0].destinations[0].amount += change;

        for (auto tx : transfers)
        {
            /* One of the transfers is too large. Retry, cutting the
               transactions into smaller pieces */
            if (wallet.txIsTooLarge(tx))
            {
                continue;
            }
        }

        sendMultipleTransactions(wallet, transfers);
        return;
    }
}

void transfer(std::shared_ptr<WalletInfo> walletInfo, uint32_t height)
{
    std::cout << InformationMsg("Note: You can type cancel at any time to "
                                "cancel the transaction")
              << std::endl << std::endl;


    uint64_t balance = walletInfo->wallet.getActualBalance();

    auto maybeAddress = getDestinationAddress();

    if (!maybeAddress.isJust)
    {
        std::cout << WarningMsg("Cancelling transaction.") << std::endl;
        return;
    }

    std::string address = maybeAddress.x;

    auto maybeAmount = getTransferAmount();

    if (!maybeAmount.isJust)
    {
        std::cout << WarningMsg("Cancelling transaction.") << std::endl;
        return;
    }

    uint64_t amount = maybeAmount.x;

    if (balance < amount)
    {
        std::cout << WarningMsg("You don't have enough funds to cover this "
                                "transaction!") << std::endl
                  << InformationMsg("Funds needed: " + formatAmount(amount))
                  << std::endl
                  << SuccessMsg("Funds available: " + formatAmount(balance))
                  << std::endl;
        return;
    }

    auto maybeFee = getFee();

    if (!maybeFee.isJust)
    {
        std::cout << WarningMsg("Cancelling transaction.") << std::endl;
        return;
    }

    uint64_t fee = maybeFee.x;

    if (balance < amount + fee)
    {
        std::cout << WarningMsg("You don't have enough funds to cover this "
                                "transaction!") << std::endl
                  << InformationMsg("Funds needed: "
                                    + formatAmount(amount + fee))
                  << std::endl
                  << SuccessMsg("Funds available: " + formatAmount(balance))
                  << std::endl;
        return;
    }

    auto maybeExtra = getPaymentID();

    if (!maybeExtra.isJust)
    {
        std::cout << WarningMsg("Cancelling transaction.") << std::endl;
        return;
    }

    std::string extra = maybeExtra.x;

    doTransfer(address, amount, fee, extra, walletInfo, height);
}

void doTransfer(std::string address, uint64_t amount, uint64_t fee,
                std::string extra, std::shared_ptr<WalletInfo> walletInfo,
                uint32_t height)
{
    uint64_t balance = walletInfo->wallet.getActualBalance();

    if (balance < amount + fee)
    {
        std::cout << WarningMsg("You don't have enough funds to cover this "
                                "transaction!") << std::endl
                  << InformationMsg("Funds needed: "
                                    + formatAmount(amount + fee))
                  << std::endl
                  << SuccessMsg("Funds available: " + formatAmount(balance))
                  << std::endl;
        return;
    }

    CryptoNote::TransactionParameters p;

    p.destinations = std::vector<CryptoNote::WalletOrder>
            {
                    {address, amount}
            };

    p.fee = fee;
    p.mixIn = CryptoNote::parameters::DEFAULT_MIXIN;
    p.extra = extra;
    p.changeDestination = walletInfo->walletAddress;

    if (!confirmTransaction(p, walletInfo))
    {
        std::cout << WarningMsg("Cancelling transaction.") << std::endl;
        return;
    }

    bool retried = false;

    while (true)
    {
        try
        {
            if (walletInfo->wallet.txIsTooLarge(p))
            {
                if (!fusionTX(walletInfo->wallet, p))
                {
                    return;
                }

                if (walletInfo->wallet.txIsTooLarge(p))
                {
                    splitTx(walletInfo->wallet, p);
                } else
                {

                    size_t id = walletInfo->wallet.transfer(p);
                    CryptoNote::WalletTransaction tx
                            = walletInfo->wallet.getTransaction(id);

                    std::cout << SuccessMsg("Transaction has been sent!")
                              << std::endl
                              << SuccessMsg("Hash:"
                                            + Common::podToHex(tx.hash))
                              << std::endl;
                }
            } else
            {
                size_t id = walletInfo->wallet.transfer(p);

                CryptoNote::WalletTransaction tx
                        = walletInfo->wallet.getTransaction(id);

                std::cout << SuccessMsg("Transaction has been sent!")
                          << std::endl
                          << SuccessMsg("Hash: " +
                                        Common::podToHex(tx.hash))
                          << std::endl;
            }
        }
        catch (const std::system_error &e)
        {
            if (retried)
            {
                std::cout << WarningMsg("Failed to send transaction!")
                          << std::endl << "Error message: " << e.what()
                          << std::endl;
                return;
            }

            bool wrongAmount = false;

            switch (e.code().value())
            {
                case WalletErrors::CryptoNote::error::WRONG_AMOUNT:
                {
                    wrongAmount = true;
                    [[fallthrough]];
                }
                case WalletErrors::CryptoNote::error::MIXIN_COUNT_TOO_BIG:
                case NodeErrors::CryptoNote::error::INTERNAL_NODE_ERROR:
                {

                    if (wrongAmount)
                    {
                        std::cout << WarningMsg("Failed to send transaction "
                                                "- not enough funds!")
                                  << std::endl
                                  << "Unable to send dust inputs."
                                  << std::endl;
                    } else
                    {
                        std::cout << WarningMsg("Failed to send transaction!")
                                  << std::endl
                                  << "Unable to find enough outputs to "
                                  << "mix with."
                                  << std::endl;
                    }

                    std::cout << "Try lowering the amount you are sending "
                              << "in one transaction." << std::endl;

                    /* Mixin 0 not allowed after MIXIN_LIMITS_V2 */
                    if (height < CryptoNote::parameters::MIXIN_LIMITS_V2_HEIGHT)
                    {
                        std::cout << "Alternatively, you can sent the mixin "
                                  << "count to 0." << std::endl;

                        if (confirm("Retry transaction with mixin of 0? "
                                    "This will compromise privacy."))
                        {
                            p.mixIn = 0;
                            retried = true;
                            continue;
                        }
                    }

                    std::cout << WarningMsg("Cancelling transaction.")
                              << std::endl;

                    break;
                }
                case NodeErrors::CryptoNote::error::NETWORK_ERROR:
                case NodeErrors::CryptoNote::error::CONNECT_ERROR:
                {
                    std::cout << WarningMsg("Couldn't connect to the network "
                                            "to send the transaction!")
                              << std::endl
                              << "Ensure TurtleCoind or the remote node you "
                              << "are using is open and functioning."
                              << std::endl;
                    break;
                }
                default:
                {
                    /* Some errors don't have an associated value, just an
                       error string */
                    std::string msg = e.what();

                    if (msg == "Failed add key input: key image already spent")
                    {
                        std::cout << WarningMsg("Failed to send transaction - "
                                                "wallet is not synced yet!")
                                  << std::endl
                                  << "Use the " << InformationMsg("bc_height")
                                  << " command to view the wallet sync status."
                                  << std::endl;
                        return;
                    }

                    std::cout << WarningMsg("Failed to send transaction!")
                              << std::endl << "Error message: " << msg
                              << std::endl
                              << "Please report what you were doing to cause "
                              << "this error so we can fix it! :)"
                              << std::endl;
                    break;
                }
            }
        }

        break;
    }
}

Maybe<std::string> getPaymentID()
{
    while (true)
    {
        std::string paymentID;

        std::cout << std::endl
                  << InformationMsg("What payment ID do you want to use?")
                  << std::endl
                  << "These are usually used for sending to exchanges."
                  << std::endl
                  << WarningMsg("Warning: if you were given a payment ID,")
                  << std::endl
                  << WarningMsg("you MUST use it, or your funds may be lost!")
                  << std::endl
                  << "Hit enter for the default of no payment ID: ";

        std::getline(std::cin, paymentID);

        if (paymentID == "")
        {
            return Just<std::string>(paymentID);
        }

        if (paymentID == "cancel")
        {
            return Nothing<std::string>();
        }

        std::vector<uint8_t> extra;

        /* Convert the payment ID into an "extra" */
        if (!CryptoNote::createTxExtraWithPaymentId(paymentID, extra))
        {
            std::cout << WarningMsg("Failed to parse! Payment ID's are 64 "
                                    "character hexadecimal strings.")
                      << std::endl;
        } else
        {
            /* Then convert the "extra" back into a string so we can pass
               the argument that walletgreen expects. Note this string is not
               the same as the original paymentID string! */
            std::string extraString;

            for (auto i : extra)
            {
                extraString += static_cast<char>(i);
            }

            return Just<std::string>(extraString);
        }
    }
}

Maybe<uint64_t> getFee()
{
    while (true)
    {
        std::string stringAmount;
        std::cout << std::endl
                  << InformationMsg("What fee do you want to use?")
                  << std::endl
                  << "Hit enter for the default fee of "
                  << formatAmount(CryptoNote::parameters::MINIMUM_FEE)
                  << ":";

        std::getline(std::cin, stringAmount);

        if (stringAmount == "")
        {
            return Just<uint64_t>(CryptoNote::parameters::MINIMUM_FEE);
        }

        if (stringAmount == "cancel")
        {
            return Nothing<uint64_t>();
        }

        uint64_t amount;

        if (parseFee(stringAmount))
        {
            parseAmount(stringAmount, amount);
            return Just<uint64_t>(amount);
        }
    }
}

Maybe<uint64_t> getTransferAmount()
{
    while (true)
    {
        std::string stringAmount;

        std::cout << std::endl
                  << InformationMsg("How much TRTL do you want to send?: ");

        std::getline(std::cin, stringAmount);

        if (stringAmount == "cancel")
        {
            return Nothing<uint64_t>();
        }

        uint64_t amount;

        if (parseAmount(stringAmount))
        {
            parseAmount(stringAmount, amount);
            return Just<uint64_t>(amount);
        }
    }
}

Maybe<std::string> getDestinationAddress()
{
    while (true)
    {
        std::string transferAddr;

        std::cout << InformationMsg("What address do you want to "
                                    "transfer to?: ");

        std::getline(std::cin, transferAddr);
        boost::algorithm::trim(transferAddr);

        if (transferAddr == "cancel")
        {
            return Nothing<std::string>();
        }

        if (parseAddress(transferAddr))
        {
            return Just<std::string>(transferAddr);
        }
    }
}

bool parseFee(std::string feeString)
{
    uint64_t fee;

    if (!parseAmount(feeString, fee))
    {
        std::cout << WarningMsg("Failed to parse fee! Ensure you entered the "
                                "value correctly.")
                  << std::endl
                  << "Please note, you can only use 2 decimal places."
                  << std::endl;

        return false;
    } else if (fee < CryptoNote::parameters::MINIMUM_FEE)
    {
        std::cout << WarningMsg("Fee must be at least 0.1 TRTL!") << std::endl;
        return false;
    }

    return true;
}


bool parseAddress(std::string address)
{
    uint64_t expectedPrefix
            = CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX;

    uint64_t prefix;

    CryptoNote::AccountPublicAddress addr;

    bool valid = CryptoNote::parseAccountAddressString(prefix, addr, address);

    /* Generate a dummy address and grab its length to check that the inputted
       address is correct */
    CryptoNote::KeyPair spendKey;
    Crypto::generate_keys(spendKey.publicKey, spendKey.secretKey);

    CryptoNote::KeyPair viewKey;
    Crypto::generate_keys(viewKey.publicKey, viewKey.secretKey);

    CryptoNote::AccountPublicAddress expectedAddr
            {spendKey.publicKey, viewKey.publicKey};

    size_t expectedLen = CryptoNote::getAccountAddressAsStr(expectedPrefix,
                                                            expectedAddr).length();

    if (address.length() != expectedLen)
    {
        std::cout << WarningMsg("Address is wrong length!") << std::endl
                  << "It should be " << expectedLen
                  << " characters long, but it is " << address.length()
                  << " characters long!" << std::endl << std::endl;

        return false;
    }
        /* We can't get the actual prefix if the address is invalid for other
           reasons. To work around this, we can just check that the address starts
           with TRTL, aslong as the prefix is the TRTL prefix. This keeps it
           working on testnets with different prefixes. */
    else if (expectedPrefix == 3914525 && address.substr(0, 4) != "TRTL")
    {
        std::cout << WarningMsg("Invalid address! It should start with TRTL!")
                  << std::endl << std::endl;
        return false;
    }
        /* We can return earlier by checking the value of valid, but then we don't
           get to give more detailed error messages about the address */
    else if (!valid)
    {
        std::cout << WarningMsg("Failed to parse address, address is not a "
                                "valid TRTL address!") << std::endl
                  << std::endl;
        return false;
    }

    return true;
}

bool parseAmount(std::string amountString)
{
    uint64_t amount;

    if (!parseAmount(amountString, amount))
    {
        std::cout << WarningMsg("Failed to parse amount! Ensure you entered "
                                "the value correctly.")
                  << std::endl
                  << "Please note, the minimum you can send is 0.01 TRTL,"
                  << std::endl
                  << "and you can only use 2 decimal places."
                  << std::endl;

        return false;
    }

    return true;
}
