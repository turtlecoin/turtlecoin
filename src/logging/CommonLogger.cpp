// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "CommonLogger.h"

namespace Logging
{
    namespace
    {
        std::string formatPattern(
            const std::string &pattern,
            const std::string &category,
            Level level,
            boost::posix_time::ptime time)
        {
            std::stringstream s;

            for (const char *p = pattern.c_str(); p && *p != 0; ++p)
            {
                if (*p == '%')
                {
                    ++p;
                    switch (*p)
                    {
                        case 0:
                            break;
                        case 'C':
                            s << category;
                            break;
                        case 'D':
                            s << time.date();
                            break;
                        case 'T':
                            s << time.time_of_day();
                            break;
                        case 'L':
                            s << std::setw(7) << std::left << ILogger::LEVEL_NAMES[level];
                            break;
                        default:
                            s << *p;
                    }
                }
                else
                {
                    s << *p;
                }
            }

            return s.str();
        }

    } // namespace

    void CommonLogger::
        operator()(const std::string &category, Level level, boost::posix_time::ptime time, const std::string &body)
    {
        if (level <= logLevel && disabledCategories.count(category) == 0)
        {
            std::string body2 = body;
            if (!pattern.empty())
            {
                size_t insertPos = 0;
                if (!body2.empty() && body2[0] == ILogger::COLOR_DELIMETER)
                {
                    size_t delimPos = body2.find(ILogger::COLOR_DELIMETER, 1);
                    if (delimPos != std::string::npos)
                    {
                        insertPos = delimPos + 1;
                    }
                }

                body2.insert(insertPos, formatPattern(pattern, category, level, time));
            }

            doLogString(body2);
        }
    }

    void CommonLogger::setPattern(const std::string &pattern)
    {
        this->pattern = pattern;
    }

    void CommonLogger::disableCategory(const std::string &category)
    {
        disabledCategories.insert(category);
    }

    void CommonLogger::setMaxLevel(Level level)
    {
        logLevel = level;
    }

    CommonLogger::CommonLogger(Level level): logLevel(level), pattern("%D %T %L [%C] ") {}

    void CommonLogger::doLogString(const std::string &message) {}

} // namespace Logging
