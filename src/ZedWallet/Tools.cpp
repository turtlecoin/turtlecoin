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

///////////////////////////////
#include <ZedWallet/Tools.h>
///////////////////////////////

#include <cctype>

#include <Common/StringTools.h>

#include <CryptoNoteCore/TransactionExtra.h>

#include <fstream>

#include <iostream>

#include <ZedWallet/ColouredMsg.h>
#include <ZedWallet/PasswordContainer.h>

void confirmPassword(std::string walletPass)
{
    /* Password container requires an rvalue, we don't want to wipe our current
       pass so copy it into a tmp string and std::move that instead */
    std::string tmpString = walletPass;
    Tools::PasswordContainer pwdContainer(std::move(tmpString));

    while (!pwdContainer.read_and_validate())
    {
        std::cout << "Incorrect password! Try again." << std::endl;
    }
}

std::string formatAmount(uint64_t amount)
{
    uint64_t dollars = amount / 100;
    uint64_t cents = amount % 100;

    return formatDollars(dollars) + "." + formatCents(cents) + " TRTL";
}

std::string formatDollars(uint64_t amount)
{
    /* We want to format our number with comma separators so it's easier to
       use. Now, we could use the nice print_money() function to do this.
       However, whilst this initially looks pretty handy, if we have a locale
       such as ja_JP.utf8, 1 TRTL will actually be formatted as 100 TRTL, which
       is terrible, and could really screw over users.

       So, easy solution right? Just use en_US.utf8! Sure, it's not very
       international, but it'll work! Unfortunately, no. The user has to have
       the locale installed, and if they don't, we get a nasty error at
       runtime.

       Annoyingly, there's no easy way to comma separate numbers outside of
       using the locale method, without writing a pretty long boiler plate
       function. So, instead, we define our own locale, which just returns
       the values we want.
       
       It's less internationally friendly than we would potentially like
       but that would require a ton of scrutinization which if not done could
       land us with quite a few issues and rightfully angry users.
       Furthermore, we'd still have to hack around cases like JP locale
       formatting things incorrectly, and it makes reading in inputs harder
       too. */

    /* Thanks to https://stackoverflow.com/a/7277333/8737306 for this neat
       workaround */
    class comma_numpunct : public std::numpunct<char>
    {
    protected:
        virtual char do_thousands_sep() const
        {
            return ',';
        }

        virtual std::string do_grouping() const
        {
            return "\03";
        }
    };

    std::locale comma_locale(std::locale(), new comma_numpunct());
    std::stringstream stream;
    stream.imbue(comma_locale);
    stream << amount;
    return stream.str();
}

/* Pad to two spaces, e.g. 5 becomes 05, 50 remains 50 */
std::string formatCents(uint64_t amount)
{
    std::stringstream stream;
    stream << std::setfill('0') << std::setw(2) << amount;
    return stream.str();
}

bool confirm(std::string msg)
{
    return confirm(msg, true);
}

/* defaultReturn = what value we return on hitting enter, i.e. the "expected"
   workflow */
bool confirm(std::string msg, bool defaultReturn)
{
    /* In unix programs, the upper case letter indicates the default, for
       example when you hit enter */
    std::string prompt = " (Y/n): ";

    /* Yes, I know I can do !defaultReturn. It doesn't make as much sense
       though. If someone deletes this comment to make me look stupid I'll be
       mad >:( */
    if (defaultReturn == false)
    {
        prompt = " (y/N): ";
    }

    while (true)
    {
        std::cout << InformationMsg(msg + prompt);

        std::string answer;
        std::getline(std::cin, answer);

        char c = std::tolower(answer[0]);

        switch (std::tolower(answer[0]))
        {
            /* Lets people spam enter / choose default value */
            case '\0':
                return defaultReturn;
            case 'y':
                return true;
            case 'n':
                return false;
        }

        std::cout << WarningMsg("Bad input: ") << InformationMsg(answer)
                  << WarningMsg(" - please enter either Y or N.")
                  << std::endl;

    }
}

std::string getPaymentID(std::string extra)
{
    std::string paymentID;

    if (extra.length() > 0)
    {
        std::vector<uint8_t> vecExtra;

        for (auto it : extra)
        {
            vecExtra.push_back(static_cast<uint8_t>(it));
        }

        Crypto::Hash paymentIdHash;

        if (CryptoNote::getPaymentIdFromTxExtra(vecExtra, paymentIdHash))
        {
            return Common::podToHex(paymentIdHash);
        }
    }

    return paymentID;
}
