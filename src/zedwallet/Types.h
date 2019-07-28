// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <config/CryptoNoteConfig.h>
#include <serialization/ISerializer.h>
#include <wallet/WalletGreen.h>

struct WalletInfo
{
    WalletInfo(
        std::string walletFileName,
        std::string walletPass,
        std::string walletAddress,
        bool viewWallet,
        CryptoNote::WalletGreen &wallet):
        walletFileName(walletFileName),
        walletPass(walletPass),
        walletAddress(walletAddress),
        viewWallet(viewWallet),
        wallet(wallet)
    {
    }

    /* How many transactions do we know about */
    size_t knownTransactionCount = 0;

    /* The wallet file name */
    std::string walletFileName;

    /* The wallet password */
    std::string walletPass;

    /* The wallets primary address */
    std::string walletAddress;

    /* Is the wallet a view only wallet */
    bool viewWallet;

    /* The walletgreen wallet container */
    CryptoNote::WalletGreen &wallet;
};

struct Config
{
    /* Was the wallet file specified on CLI */
    bool walletGiven = false;

    /* Was the wallet pass specified on CLI */
    bool passGiven = false;

    /* Should we log walletd logs to a file */
    bool debug = false;

    /* The daemon host */
    std::string host = "127.0.0.1";

    /* The daemon port */
    int port = CryptoNote::RPC_DEFAULT_PORT;

    /* The wallet file path */
    std::string walletFile = "";

    /* The wallet password */
    std::string walletPass = "";
};

struct AddressBookEntry
{
    AddressBookEntry() {}

    /* Used for quick comparison with strings */
    AddressBookEntry(std::string friendlyName): friendlyName(friendlyName) {}

    AddressBookEntry(std::string friendlyName, std::string address, std::string paymentID, bool integratedAddress):
        friendlyName(friendlyName),
        address(address),
        paymentID(paymentID),
        integratedAddress(integratedAddress)
    {
    }

    /* Friendly name for this address book entry */
    std::string friendlyName;

    /* The wallet address of this entry */
    std::string address;

    /* The payment ID associated with this address */
    std::string paymentID;

    /* Did the user enter this as an integrated address? (We need this to
       display back the address as either an integrated address, or an
       address + payment ID pair */
    bool integratedAddress;

    void serialize(CryptoNote::ISerializer &s)
    {
        KV_MEMBER(friendlyName)
        KV_MEMBER(address)
        KV_MEMBER(paymentID)
        KV_MEMBER(integratedAddress)
    }

    /* Only compare via name as we don't really care about the contents */
    bool operator==(const AddressBookEntry &rhs) const
    {
        return rhs.friendlyName == friendlyName;
    }
};

/* An address book is a vector of address book entries */
typedef std::vector<AddressBookEntry> AddressBook;

/* This borrows from haskell, and is a nicer boost::optional class. We either
   have Just a value, or Nothing.

   Example usage follows.
   The below code will print:

   ```
   100
   Nothing
   ```

   Maybe<int> parseAmount(std::string input)
   {
        if (input.length() == 0)
        {
            return Nothing<int>();
        }

        try
        {
            return Just<int>(std::stoi(input)
        }
        catch (const std::invalid_argument &)
        {
            return Nothing<int>();
        }
   }

   int main()
   {
       auto foo = parseAmount("100");

       if (foo.isJust)
       {
           std::cout << foo.x << std::endl;
       }
       else
       {
           std::cout << "Nothing" << std::endl;
       }

       auto bar = parseAmount("garbage");

       if (bar.isJust)
       {
           std::cout << bar.x << std::endl;
       }
       else
       {
           std::cout << "Nothing" << std::endl;
       }
   }

*/

template<class X> struct Maybe
{
    X x;

    bool isJust;

    Maybe(const X &x): x(x), isJust(true) {}

    Maybe(): isJust(false) {}
};

template<class X> Maybe<X> Just(const X &x)
{
    return Maybe<X>(x);
}

template<class X> Maybe<X> Nothing()
{
    return Maybe<X>();
}
