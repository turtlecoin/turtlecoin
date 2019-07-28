// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "LoggerGroup.h"

#include <algorithm>

namespace Logging
{
    LoggerGroup::LoggerGroup(Level level): CommonLogger(level) {}

    void LoggerGroup::addLogger(ILogger &logger)
    {
        loggers.push_back(&logger);
    }

    void LoggerGroup::
        operator()(const std::string &category, Level level, boost::posix_time::ptime time, const std::string &body)
    {
        if (level <= logLevel && disabledCategories.count(category) == 0)
        {
            for (auto &logger : loggers)
            {
                (*logger)(category, level, time, body);
            }
        }
    }

} // namespace Logging
