// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <fstream>
#include "StreamLogger.h"

namespace Logging
{

    class FileLogger : public StreamLogger
    {
    public:
        FileLogger(Level level = DEBUGGING);

        void init(const std::string &filename);

    private:
        std::ofstream fileStream;
    };

}
