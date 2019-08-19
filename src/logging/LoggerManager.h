// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "../common/JsonValue.h"
#include "LoggerGroup.h"

#include <list>
#include <memory>
#include <mutex>

namespace Logging
{
    class LoggerManager : public LoggerGroup
    {
      public:
        LoggerManager();

        void configure(const Common::JsonValue &val);

        virtual void
            operator()(const std::string &category, Level level, boost::posix_time::ptime time, const std::string &body)
                override;

      private:
        std::vector<std::unique_ptr<CommonLogger>> loggers;

        std::mutex reconfigureLock;
    };

} // namespace Logging
