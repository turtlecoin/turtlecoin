// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CommonLogger.h"

#include <vector>

namespace Logging
{
    class LoggerGroup : public CommonLogger
    {
      public:
        LoggerGroup(Level level = DEBUGGING);

        void addLogger(ILogger &logger);

        void removeLogger(ILogger &logger);

        virtual void
            operator()(const std::string &category, Level level, boost::posix_time::ptime time, const std::string &body)
                override;

      protected:
        std::vector<ILogger *> loggers;
    };

} // namespace Logging
