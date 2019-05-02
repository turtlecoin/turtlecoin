// Copyright (c) 2019, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#include <string>

#include <vector>

namespace Utilities
{
    void removeCharFromString(std::string &str, const char c);

    void trim(std::string &str);

    void leftTrim(std::string &str);

    void rightTrim(std::string &str);

    bool startsWith(const std::string &str, const std::string &substring);

    std::vector<std::string> split(const std::string& str, char delim);

    std::string removePrefix(const std::string &str, const std::string &prefix);
}
