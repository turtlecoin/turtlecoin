// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <list>
#include <memory>
#include <mutex>
#include "../Common/JsonValue.h"
#include "LoggerGroup.h"

namespace Logging
{

    class LoggerManager : public LoggerGroup
    {
    public:
        LoggerManager();

        void configure(const Common::JsonValue &val);

        virtual void operator()(const std::string &category, Level level, boost::posix_time::ptime time,
                                const std::string &body) override;

    private:
        std::vector<std::unique_ptr<CommonLogger>> loggers;
        std::mutex reconfigureLock;
    };

}
