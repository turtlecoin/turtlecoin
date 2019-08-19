// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "ErrorMessage.h"

#include <cerrno>
#include <cstring>

namespace System
{
    std::string lastErrorMessage()
    {
        return errorMessage(errno);
    }

    std::string errorMessage(int err)
    {
        return "result=" + std::to_string(err) + ", " + std::strerror(err);
    }

} // namespace System
