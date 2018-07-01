// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <chrono>
#include <System/ContextGroup.h>
#include <System/Timer.h>

namespace System
{

    class ContextGroupTimeout
    {
    public:
        ContextGroupTimeout(Dispatcher &, ContextGroup &, std::chrono::nanoseconds);

    private:
        Timer timeoutTimer;
        ContextGroup workingContextGroup;
    };

}
