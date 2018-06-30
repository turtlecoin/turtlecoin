// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <iostream>
#include "ILogger.h"

namespace Logging
{

    class LoggerMessage : public std::ostream, std::streambuf
    {
    public:
        LoggerMessage(ILogger &logger, const std::string &category, Level level, const std::string &color);

        ~LoggerMessage();

        LoggerMessage(const LoggerMessage &) = delete;

        LoggerMessage &operator=(const LoggerMessage &) = delete;

        LoggerMessage(LoggerMessage &&other);

    private:
        int sync() override;

        std::streamsize xsputn(const char *s, std::streamsize n) override;

        int overflow(int c) override;

        std::string message;
        const std::string category;
        Level logLevel;
        ILogger &logger;
        boost::posix_time::ptime timestamp;
        bool gotText;
    };

}
