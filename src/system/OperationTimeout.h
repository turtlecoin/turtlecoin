// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <system/ContextGroup.h>
#include <system/Dispatcher.h>
#include <system/Timer.h>

namespace System
{
    template<typename T> class OperationTimeout
    {
      public:
        OperationTimeout(Dispatcher &dispatcher, T &object, std::chrono::nanoseconds timeout):
            object(object),
            timerContext(dispatcher),
            timeoutTimer(dispatcher)
        {
            timerContext.spawn([this, timeout]() {
                try
                {
                    timeoutTimer.sleep(timeout);
                    timerContext.interrupt();
                }
                catch (...)
                {
                }
            });
        }

        ~OperationTimeout()
        {
            timerContext.interrupt();
            timerContext.wait();
        }

      private:
        T &object;

        ContextGroup timerContext;

        Timer timeoutTimer;
    };

} // namespace System
