// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CommonLogger.h"

#include <mutex>

namespace Logging
{
    class ConsoleLogger : public CommonLogger
    {
      public:
        ConsoleLogger(Level level = DEBUGGING);

      protected:
        virtual void doLogString(const std::string &message) override;

      private:
        std::mutex mutex;
    };

} // namespace Logging
