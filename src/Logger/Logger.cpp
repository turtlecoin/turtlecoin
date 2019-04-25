// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

//////////////////////////
#include <Logger/Logger.h>
//////////////////////////

#include <ctime>

#include <iomanip>

#include <iostream>

#include <sstream>

namespace Logger
{
    Logger logger;

    std::string logLevelToString(const LogLevel level)
    {
        switch (level)
        {
            case DISABLED:
            {
                return "Disabled";
            }
            case DEBUG:
            {
                return "Debug";
            }
            case INFO:
            {
                return "Info";
            }
            case WARNING:
            {
                return "Warning";
            }
            case FATAL:
            {
                return "Fatal";
            }
        }
    }

    std::string logCategoryToString(const LogCategory category)
    {
        switch(category)
        {
            case SYNC:
            {
                return "Sync";
            }
            case TRANSACTIONS:
            {
                return "Transactions";
            }
            case FILESYSTEM:
            {
                return "Filesystem";
            }
            case SAVE:
            {
                return "Save";
            }
            case DAEMON:
            {
                return "Daemon";
            }
        }
    }

    void Logger::log(
        const std::string message,
        const LogLevel level,
        const std::vector<LogCategory> categories) const
    {
        if (level == DISABLED)
        {
            return;
        }

        const std::time_t now = std::time(nullptr);

        std::stringstream output;

        output << "[" << std::put_time(std::localtime(&now), "%H:%M:%S") << "] "
               << "[" << logLevelToString(level) << "]";

        for (const auto &category : categories)
        {
            output << " [" << logCategoryToString(category) << "]";
        }

        output << ": " << message;

        if (level <= m_logLevel)
        {
            /* If the user provides a callback, log to that instead */
            if (m_callback)
            {
                m_callback(output.str(), message, level, categories);
            }
            else
            {
                std::cout << output.str() << std::endl;
            }
        }
    }

    void Logger::setLogLevel(const LogLevel level)
    {
        m_logLevel = level;
    }

    void Logger::setLogCallback(
        std::function<void(
            const std::string prettyMessage,
            const std::string message,
            const LogLevel level,
            const std::vector<LogCategory> categories)> callback)
    {
        m_callback = callback;
    }
}
