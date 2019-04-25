// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <functional>

#include <string>

#include <vector>

namespace Logger
{
    enum LogLevel
    {
        DEBUG = 4,
        INFO = 3,
        WARNING = 2,
        FATAL = 1,
        DISABLED = 0,
    };

    enum LogCategory
    {
        SYNC,
        TRANSACTIONS,
        FILESYSTEM,
        SAVE,
        DAEMON,
    };

    class Logger
    {
        public:
            Logger() {};

            void log(
                const std::string message,
                const LogLevel level,
                const std::vector<LogCategory> categories) const;

            void setLogLevel(const LogLevel level);

            void setLogCallback(
                std::function<void(
                    const std::string prettyMessage,
                    const std::string message,
                    const LogLevel level,
                    const std::vector<LogCategory> categories)> callback);

        private:
            /* Logging disabled by default */
            LogLevel m_logLevel = DISABLED;

            std::function<void(
                const std::string prettyMessage,
                const std::string message,
                const LogLevel level,
                const std::vector<LogCategory> categories)> m_callback;
    };

    /* Global logger instance */
    extern Logger logger;
}
