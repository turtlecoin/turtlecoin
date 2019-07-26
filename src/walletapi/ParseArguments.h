// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <config/CryptoNoteConfig.h>

#include <logger/Logger.h>

struct ApiConfig
{
    /* The IP to listen for requests on */
    std::string rpcBindIp;

    /* What port should we listen on */
    uint16_t port;

    /* Password the user must supply with each request */
    std::string rpcPassword;

    /* The value to use with the 'Access-Control-Allow-Origin' header */
    std::string corsHeader;

    /* Controls what level of messages to log */
    Logger::LogLevel logLevel = Logger::DISABLED;

    unsigned int threads;
};

ApiConfig parseArguments(int argc, char **argv);
