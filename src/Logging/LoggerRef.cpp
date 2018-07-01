// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "LoggerRef.h"

namespace Logging
{

    LoggerRef::LoggerRef(ILogger &logger, const std::string &category) : logger(&logger), category(category)
    {
    }

    LoggerMessage LoggerRef::operator()(Level level, const std::string &color) const
    {
        return LoggerMessage(*logger, category, level, color);
    }

    ILogger &LoggerRef::getLogger() const
    {
        return *logger;
    }

}
