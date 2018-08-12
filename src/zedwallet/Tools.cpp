// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

////////////////////////////
#include <zedwallet/Tools.h>
////////////////////////////

#include <cmath>

#include <Common/Base58.h>
#include <Common/StringTools.h>

#include <CryptoNoteCore/CryptoNoteBasicImpl.h>
#include <CryptoNoteCore/CryptoNoteTools.h>
#include <CryptoNoteCore/TransactionExtra.h>

#include <zedwallet/ColouredMsg.h>
#include <zedwallet/PasswordContainer.h>
#include <zedwallet/WalletConfig.h>

void confirmPassword(std::string walletPass, std::string msg)
{
    /* Password container requires an rvalue, we don't want to wipe our current
       pass so copy it into a tmp string and std::move that instead */
    std::string tmpString = walletPass;
    Tools::PasswordContainer pwdContainer(std::move(tmpString));

    while (!pwdContainer.read_and_validate(msg))
    {
        std::cout << WarningMsg("Incorrect password! Try again.") << std::endl;
    }
}

/* Get the amount we need to divide to convert from atomic to pretty print,
   e.g. 100 for 2 decimal places */
uint64_t getDivisor()
{
    return static_cast<uint64_t>(pow(10, WalletConfig::numDecimalPlaces));
}

std::string formatAmount(uint64_t amount)
{
    const uint64_t divisor = getDivisor();
    const uint64_t dollars = amount / divisor;
    const uint64_t cents = amount % divisor;

    return formatDollars(dollars) + "." + formatCents(cents) + " "
         + WalletConfig::ticker;
}

std::string formatAmountBasic(uint64_t amount)
{
    const uint64_t divisor = getDivisor();
    const uint64_t dollars = amount / divisor;
    const uint64_t cents = amount % divisor;

    return std::to_string(dollars) + "." + formatCents(cents);
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

/* Pad to the amount of decimal spaces, e.g. with 2 decimal spaces 5 becomes
   05, 50 remains 50 */
std::string formatCents(uint64_t amount)
{
    std::stringstream stream;
    stream << std::setfill('0') << std::setw(WalletConfig::numDecimalPlaces)
           << amount;
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

        const char c = std::tolower(answer[0]);

        switch(c)
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

std::string getPaymentIDFromExtra(std::string extra)
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

/* Note: this is not portable, it only works with terminals that support ANSI
   codes (e.g., not Windows) */
std::string yellowANSIMsg(std::string msg)
{
    const std::string CYELLOW = "\033[1;33m";
    const std::string RESET = "\033[0m";
    return CYELLOW + msg + RESET;
}

std::string getPrompt(std::shared_ptr<WalletInfo> &walletInfo)
{
    const int promptLength = 20;
    const std::string extension = ".wallet";

    std::string walletName = walletInfo->walletFileName;

    /* Filename ends in .wallet, remove extension */
    if (std::equal(extension.rbegin(), extension.rend(), 
                   walletInfo->walletFileName.rbegin()))
    {
        const size_t extPos = walletInfo->walletFileName.find_last_of('.');

        walletName = walletInfo->walletFileName.substr(0, extPos);
    }

    const std::string shortName = walletName.substr(0, promptLength);

    return "[" + WalletConfig::ticker + " " + shortName + "]: ";
}

std::string unixTimeToDate(uint64_t timestamp)
{
    const std::time_t time = timestamp;
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%F %R", std::localtime(&time));
    return std::string(buffer);
}

std::string createIntegratedAddress(std::string address, std::string paymentID)
{
    uint64_t prefix;

    CryptoNote::AccountPublicAddress addr;

    /* Get the private + public key from the address */
    const bool valid = CryptoNote::parseAccountAddressString(prefix, addr,
                                                             address);

    /* Pack as a binary array */
    CryptoNote::BinaryArray ba;
    CryptoNote::toBinaryArray(addr, ba);
    std::string keys = Common::asString(ba);

    /* Encode prefix + paymentID + keys as an address */
    return Tools::Base58::encode_addr
    (
        CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
        paymentID + keys
    );
}
