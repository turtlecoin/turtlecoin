// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CommonLogger.h"

#include <mutex>

namespace Logging
{
    class StreamLogger : public CommonLogger
    {
      public:
        StreamLogger(Level level = DEBUGGING);

        StreamLogger(std::ostream &stream, Level level = DEBUGGING);

        void attachToStream(std::ostream &stream);

      protected:
        virtual void doLogString(const std::string &message) override;

      protected:
        std::ostream *stream;

      private:
        std::mutex mutex;
    };

} // namespace Logging
