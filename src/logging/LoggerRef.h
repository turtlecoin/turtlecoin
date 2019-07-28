// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "ILogger.h"
#include "LoggerMessage.h"

namespace Logging
{
    class LoggerRef
    {
      public:
        LoggerRef(std::shared_ptr<ILogger> logger, const std::string &category);

        LoggerMessage operator()(Level level = INFO, const std::string &color = DEFAULT) const;

        std::shared_ptr<ILogger> getLogger() const;

      private:
        std::shared_ptr<ILogger> logger;

        std::string category;
    };

} // namespace Logging
