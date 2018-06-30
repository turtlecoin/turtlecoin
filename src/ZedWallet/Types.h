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

#pragma once

#include <Wallet/WalletGreen.h>

struct WalletInfo
{
    WalletInfo(std::string walletFileName,
               std::string walletPass,
               std::string walletAddress,
               bool viewWallet,
               CryptoNote::WalletGreen &wallet) :
            walletFileName(walletFileName),
            walletPass(walletPass),
            walletAddress(walletAddress),
            viewWallet(viewWallet),
            wallet(wallet)
    {}

    size_t knownTransactionCount = 0;

    std::string walletFileName;
    std::string walletPass;
    std::string walletAddress;

    bool viewWallet;

    CryptoNote::WalletGreen &wallet;
};

struct Config
{
    bool exit;

    bool walletGiven;
    bool passGiven;

    std::string host;
    int port;

    std::string walletFile;
    std::string walletPass;
};

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

template<class X>
struct Maybe
{
    X x;
    bool isJust;

    Maybe(const X &x) : x(x), isJust(true)
    {}

    Maybe() : isJust(false)
    {}
};

template<class X>
Maybe<X> Just(const X &x)
{
    return Maybe<X>(x);
}

template<class X>
Maybe<X> Nothing()
{
    return Maybe<X>();
}
