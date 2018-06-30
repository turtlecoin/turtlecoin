// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "MinerConfig.h"

#include "Common/CommandLine.h"

namespace CryptoNote
{

    namespace
    {
        const command_line::arg_descriptor<std::string> arg_extra_messages = {"extra-messages-file",
                                                                              "Specify file for extra messages to include into coinbase transactions",
                                                                              "", true};
        const command_line::arg_descriptor<std::string> arg_start_mining = {"start-mining",
                                                                            "Specify wallet address to mining for", "",
                                                                            true};
        const command_line::arg_descriptor<uint32_t> arg_mining_threads = {"mining-threads",
                                                                           "Specify mining threads count", 0, true};
    }

    MinerConfig::MinerConfig()
    {
        miningThreads = 0;
    }

    void MinerConfig::initOptions(boost::program_options::options_description &desc)
    {
        command_line::add_arg(desc, arg_extra_messages);
        command_line::add_arg(desc, arg_start_mining);
        command_line::add_arg(desc, arg_mining_threads);
    }

    void MinerConfig::init(const boost::program_options::variables_map &options)
    {
        if (command_line::has_arg(options, arg_extra_messages))
        {
            extraMessages = command_line::get_arg(options, arg_extra_messages);
        }

        if (command_line::has_arg(options, arg_start_mining))
        {
            startMining = command_line::get_arg(options, arg_start_mining);
        }

        if (command_line::has_arg(options, arg_mining_threads))
        {
            miningThreads = command_line::get_arg(options, arg_mining_threads);
        }
    }

} //namespace CryptoNote
