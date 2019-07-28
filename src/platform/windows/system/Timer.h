// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <chrono>

namespace System
{
    class Dispatcher;

    class Timer
    {
      public:
        Timer();

        explicit Timer(Dispatcher &dispatcher);

        Timer(const Timer &) = delete;

        Timer(Timer &&other);

        ~Timer();

        Timer &operator=(const Timer &) = delete;

        Timer &operator=(Timer &&other);

        void sleep(std::chrono::nanoseconds duration);

      private:
        Dispatcher *dispatcher;

        void *context;
    };

} // namespace System
