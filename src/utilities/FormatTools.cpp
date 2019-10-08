// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.


//////////////////////////////////
#include <utilities/FormatTools.h>
//////////////////////////////////

#include <config/CryptoNoteConfig.h>
#include <config/WalletConfig.h>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <rpc/CoreRpcServerCommandsDefinitions.h>

namespace Utilities
{
    std::string get_mining_speed(const uint64_t hashrate)
    {
        std::stringstream stream;

        stream << std::setprecision(2) << std::fixed;

        if (hashrate > 1e9)
        {
            stream << hashrate / 1e9 << " GH/s";
        }
        else if (hashrate > 1e6)
        {
            stream << hashrate / 1e6 << " MH/s";
        }
        else if (hashrate > 1e3)
        {
            stream << hashrate / 1e3 << " KH/s";
        }
        else
        {
            stream << hashrate << " H/s";
        }

        return stream.str();
    }

    std::string get_sync_percentage(uint64_t height, const uint64_t target_height)
    {
        /* Don't divide by zero */
        if (height == 0 || target_height == 0)
        {
            return "0.00";
        }

        /* So we don't have > 100% */
        if (height > target_height)
        {
            height = target_height;
        }

        float percent = 100.0f * height / target_height;

        if (height < target_height && percent > 99.99f)
        {
            percent = 99.99f; // to avoid 100% when not fully synced
        }

        std::stringstream stream;

        stream << std::setprecision(2) << std::fixed << percent;

        return stream.str();
    }

    ForkStatus get_fork_status(
        const uint64_t height,
        const std::vector<uint64_t> upgrade_heights,
        const uint64_t supported_height)
    {
        /* Allow fork heights to be empty */
        if (upgrade_heights.empty())
        {
            return UpToDate;
        }

        uint64_t next_fork = 0;

        for (auto upgrade : upgrade_heights)
        {
            /* We have hit an upgrade already that the user cannot support */
            if (height >= upgrade && supported_height < upgrade)
            {
                return OutOfDate;
            }

            /* Get the next fork height */
            if (upgrade > height)
            {
                next_fork = upgrade;
                break;
            }
        }

        const float days = (next_fork - height) / CryptoNote::parameters::EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;

        /* Next fork in < 30 days away */
        if (days < 30)
        {
            /* Software doesn't support the next fork yet */
            if (supported_height < next_fork)
            {
                return ForkSoonNotReady;
            }
            else
            {
                return ForkSoonReady;
            }
        }

        if (height > next_fork)
        {
            return UpToDate;
        }

        return ForkLater;
    }

    std::string get_fork_time(const uint64_t height, const std::vector<uint64_t> upgrade_heights)
    {
        uint64_t next_fork = 0;

        for (auto upgrade : upgrade_heights)
        {
            /* Get the next fork height */
            if (upgrade > height)
            {
                next_fork = upgrade;
                break;
            }
        }

        const float days =
            (static_cast<float>(next_fork - height) / CryptoNote::parameters::EXPECTED_NUMBER_OF_BLOCKS_PER_DAY);

        std::stringstream stream;

        stream << std::setprecision(2) << std::fixed;

        if (next_fork == 0)
        {
            stream << "No Fork Planned";
        }
        if (height == next_fork)
        {
            stream << "Now!";
        }
        else if (days < 1)
        {
            stream << days * 24 << " Hours";
        }
        else
        {
            stream << days << " Days";
        }

        return stream.str();
    }

    std::string get_update_status(const ForkStatus forkStatus)
    {
        switch (forkStatus)
        {
            case UpToDate:
            case ForkLater:
            {
                return "Up To Date";
            }
            case ForkSoonReady:
            {
                return "Forking Soon";
            }
            case ForkSoonNotReady:
            {
                return "Update Needed";
            }
            case OutOfDate:
            {
                return "Likely Forked";
            }
            default:
            {
                throw std::runtime_error("Unexpected case unhandled");
            }
        }
    }

    std::string get_upgrade_info(const uint64_t supported_height, const std::vector<uint64_t> upgrade_heights)
    {
        for (auto upgrade : upgrade_heights)
        {
            if (upgrade > supported_height)
            {
                return "The network forked at height " + std::to_string(upgrade)
                       + ", please update your software: " + CryptoNote::LATEST_VERSION_URL;
            }
        }

        /* This shouldnt happen */
        return std::string();
    }

    /* Get the amount we need to divide to convert from atomic to pretty print,
       e.g. 100 for 2 decimal places */
    uint64_t getDivisor()
    {
        return static_cast<uint64_t>(pow(10, WalletConfig::numDecimalPlaces));
    }

    std::string formatDollars(const uint64_t amount)
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
    std::string formatCents(const uint64_t amount)
    {
        std::stringstream stream;
        stream << std::setfill('0') << std::setw(WalletConfig::numDecimalPlaces) << amount;
        return stream.str();
    }

    std::string formatAmount(const uint64_t amount)
    {
        const uint64_t divisor = getDivisor();
        const uint64_t dollars = amount / divisor;
        const uint64_t cents = amount % divisor;

        return formatDollars(dollars) + "." + formatCents(cents) + " " + WalletConfig::ticker;
    }

    std::string formatAmountBasic(const uint64_t amount)
    {
        const uint64_t divisor = getDivisor();
        const uint64_t dollars = amount / divisor;
        const uint64_t cents = amount % divisor;

        return std::to_string(dollars) + "." + formatCents(cents);
    }

    std::string prettyPrintBytes(uint64_t input)
    {
        /* Store as a double so we can have 12.34 kb for example */
        double numBytes = static_cast<double>(input);

        std::vector<std::string> suffixes = {"B", "KB", "MB", "GB", "TB"};

        uint64_t selectedSuffix = 0;

        while (numBytes >= 1024 && selectedSuffix < suffixes.size() - 1)
        {
            selectedSuffix++;

            numBytes /= 1024;
        }

        std::stringstream msg;

        msg << std::fixed << std::setprecision(2) << numBytes << " " << suffixes[selectedSuffix];

        return msg.str();
    }

    std::string unixTimeToDate(const uint64_t timestamp)
    {
        const std::time_t time = timestamp;
        char buffer[100];
        std::strftime(buffer, sizeof(buffer), "%F %R", std::localtime(&time));
        return std::string(buffer);
    }

} // namespace Utilities
