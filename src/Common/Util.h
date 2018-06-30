// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <string>
#include <system_error>

namespace Tools
{
    std::string getDefaultDataDirectory();

    std::string getDefaultCacheFile(const std::string &dataDir);

    std::string get_os_version_string();

    bool create_directories_if_necessary(const std::string &path);

    std::error_code replace_file(const std::string &replacement_name, const std::string &replaced_name);

    bool directoryExists(const std::string &path);
}
