// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "CommandLine.h"

namespace command_line
{
    const arg_descriptor<bool> arg_help = {"help", "Produce help message"};
    const arg_descriptor<bool> arg_version = {"version", "Output version information"};
    const arg_descriptor<std::string> arg_data_dir = {"data-dir", "Specify data directory"};
}
