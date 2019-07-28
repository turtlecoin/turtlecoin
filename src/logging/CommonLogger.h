// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "ILogger.h"

#include <set>

namespace Logging
{
    class CommonLogger : public ILogger
    {
      public:
        virtual ~CommonLogger() {};

        virtual void
            operator()(const std::string &category, Level level, boost::posix_time::ptime time, const std::string &body)
                override;

        virtual void disableCategory(const std::string &category);

        virtual void setMaxLevel(Level level);

        void setPattern(const std::string &pattern);

      protected:
        std::set<std::string> disabledCategories;

        Level logLevel;

        std::string pattern;

        CommonLogger(Level level);

        virtual void doLogString(const std::string &message);
    };

} // namespace Logging
