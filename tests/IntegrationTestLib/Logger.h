// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#ifdef LOG_ERROR
#undef LOG_ERROR
#endif

#ifdef LOG_WARNING
#undef LOG_WARNING
#endif

#include <iostream>
#include <boost/lexical_cast.hpp>
#include <map>
#include <mutex>

#ifdef _WIN32
#define __FUNCTION_SIGNATURE__ __FUNCSIG__
#else
#define __FUNCTION_SIGNATURE__ __PRETTY_FUNCTION__
#endif

#define LOG_(str, lvl, idnt) (CLogger::Instance().Log((std::string("")+(str)), (lvl), (idnt)))
#define LOG_VERBOSE(str) LOG_((str), (CLogger::VERBOSE),0 )
#define LOG_TRACE(str) LOG_((str), (CLogger::TRACE),0 )
#define LOG_DEBUG(str) LOG_((str), (CLogger::DEBUG),0 )
#define LOG_ERROR(str) LOG_((str), (CLogger::_ERROR), 0 )
#define LOG_WARNING(str) LOG_((str), (CLogger::WARNING),0 )

#define TO_STRING(param) boost::lexical_cast<std::string>((param))


class CLogger
{
public:
    enum LOG_LEVEL
    {
        VERBOSE,
        DEBUG,
        TRACE,
        WARNING,
        _ERROR
    };

    static CLogger &Instance();

    void init(LOG_LEVEL log_lvl);

    void Log(const std::string &log_info, LOG_LEVEL log_lvl, int indent_inc = 0);

private:
    int indent;
    std::map<LOG_LEVEL, std::string> level_names;
    LOG_LEVEL log_level;
    std::mutex mutex;

    CLogger()
    {};

    CLogger(const CLogger &root);

    CLogger &operator=(const CLogger &);
};
