// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "MiningConfig.h"

#include "common/StringTools.h"
#include "logging/ILogger.h"
#include "version.h"

#include <algorithm>
#include <common/Util.h>
#include <config/CliHeader.h>
#include <config/CryptoNoteConfig.h>
#include <cxxopts.hpp>
#include <errors/ValidateParameters.h>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <thread>
#include <utilities/ColouredMsg.h>
#include <utilities/String.h>
#include <utilities/Utilities.h>
#include <vector>

namespace CryptoNote
{
    namespace
    {
        const size_t CONCURRENCY_LEVEL = std::thread::hardware_concurrency();
    }

    MiningConfig::MiningConfig(): help(false), version(false) {}

    void MiningConfig::parse(int argc, char **argv)
    {
        cxxopts::Options options(argv[0], getProjectCLIHeader());

        options.add_options("Core")(
            "help", "Display this help message", cxxopts::value<bool>(help)->implicit_value("true"))(
            "version",
            "Output software version information",
            cxxopts::value<bool>(version)->default_value("false")->implicit_value("true"));

        options.add_options("Daemon")(
            "daemon-address",
            "The daemon [host:port] combination to use for node operations. This option overrides --daemon-host and "
            "--daemon-rpc-port",
            cxxopts::value<std::string>(daemonAddress),
            "<host:port>")(
            "daemon-host",
            "The daemon host to use for node operations",
            cxxopts::value<std::string>(daemonHost)->default_value("127.0.0.1"),
            "<host>")(
            "daemon-rpc-port",
            "The daemon RPC port to use for node operations",
            cxxopts::value<uint16_t>(daemonPort)->default_value(std::to_string(CryptoNote::RPC_DEFAULT_PORT)),
            "#")(
            "scan-time",
            "Blockchain polling interval (seconds). How often miner will check the Blockchain for updates",
            cxxopts::value<size_t>(scanPeriod)->default_value("1"),
            "#");

        options.add_options("Mining")(
            "address", "The valid CryptoNote miner's address", cxxopts::value<std::string>(miningAddress), "<address>")(
            "block-timestamp-interval",
            "Timestamp incremental step for each subsequent block. May be set only if --first-block-timestamp has been "
            "set.",
            cxxopts::value<int64_t>(blockTimestampInterval)->default_value("0"),
            "#")(
            "first-block-timestamp",
            "Set timestamp to the first mined block. 0 means leave timestamp unchanged",
            cxxopts::value<uint64_t>(firstBlockTimestamp)->default_value("0"),
            "#")(
            "limit",
            "Mine this exact quantity of blocks and then stop. 0 means no limit",
            cxxopts::value<size_t>(blocksLimit)->default_value("0"),
            "#")(
            "threads",
            "The mining threads count. Must not exceed hardware capabilities.",
            cxxopts::value<size_t>(threadCount)->default_value(std::to_string(CONCURRENCY_LEVEL)),
            "#");

        try
        {
            auto result = options.parse(argc, argv);
        }
        catch (const cxxopts::OptionException &e)
        {
            std::cout << WarningMsg("Error: Unable to parse command line argument options: ") << WarningMsg(e.what())
                      << "\n\n";
            std::cout << options.help({}) << std::endl;
            exit(1);
        }

        if (help) // Do we want to display the help message?
        {
            std::cout << options.help({}) << std::endl;
            exit(0);
        }
        else if (version) // Do we want to display the software version?
        {
            std::cout << InformationMsg(getProjectCLIHeader()) << std::endl;
            exit(0);
        }

        const bool integratedAddressesAllowed = false;

        Error error = validateAddresses({miningAddress}, integratedAddressesAllowed);

        while (error != SUCCESS)
        {
            /* If they didn't enter an address, don't report an error. Probably just
             unaware of cli options. */
            if (!miningAddress.empty())
            {
                std::cout << WarningMsg("Address is not valid: ") << WarningMsg(error) << std::endl;
            }

            std::cout << InformationMsg("What address do you want to mine to?: ");
            std::getline(std::cin, miningAddress);
            Utilities::trim(miningAddress);

            error = validateAddresses({miningAddress}, integratedAddressesAllowed);
        }

        if (!daemonAddress.empty())
        {
            if (!Utilities::parseDaemonAddressFromString(daemonHost, daemonPort, daemonAddress))
            {
                throw std::runtime_error("Could not parse --daemon-address option");
            }
        }

        if (threadCount == 0 || threadCount > CONCURRENCY_LEVEL)
        {
            throw std::runtime_error("--threads option must be 1.." + std::to_string(CONCURRENCY_LEVEL));
        }

        if (scanPeriod == 0)
        {
            throw std::runtime_error("--scan-time must not be zero");
        }

        if (firstBlockTimestamp == 0 && blockTimestampInterval != 0)
        {
            throw std::runtime_error(
                "If you specify --block-timestamp-interval you must also specify --first-block-timestamp");
        }
    }

} // namespace CryptoNote
