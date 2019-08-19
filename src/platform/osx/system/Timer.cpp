// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "Timer.h"

#include "Dispatcher.h"

#include <cassert>
#include <stdexcept>
#include <string>
#include <sys/errno.h>
#include <sys/event.h>
#include <sys/time.h>
#include <system/ErrorMessage.h>
#include <system/InterruptedException.h>
#include <unistd.h>

namespace System
{
    Timer::Timer(): dispatcher(nullptr) {}

    Timer::Timer(Dispatcher &dispatcher): dispatcher(&dispatcher), context(nullptr), timer(-1) {}

    Timer::Timer(Timer &&other): dispatcher(other.dispatcher)
    {
        if (other.dispatcher != nullptr)
        {
            assert(other.context == nullptr);
            timer = other.timer;
            context = nullptr;
            other.dispatcher = nullptr;
        }
    }

    Timer::~Timer()
    {
        assert(dispatcher == nullptr || context == nullptr);
    }

    Timer &Timer::operator=(Timer &&other)
    {
        assert(dispatcher == nullptr || context == nullptr);
        dispatcher = other.dispatcher;
        if (other.dispatcher != nullptr)
        {
            assert(other.context == nullptr);
            timer = other.timer;
            context = nullptr;
            other.dispatcher = nullptr;
            other.timer = -1;
        }

        return *this;
    }

    void Timer::sleep(std::chrono::nanoseconds duration)
    {
        assert(dispatcher != nullptr);
        assert(context == nullptr);
        if (dispatcher->interrupted())
        {
            throw InterruptedException();
        }

        OperationContext timerContext;
        timerContext.context = dispatcher->getCurrentContext();
        timerContext.interrupted = false;
        timer = dispatcher->getTimer();

        struct kevent event;
        EV_SET(
            &event,
            timer,
            EVFILT_TIMER,
            EV_ADD | EV_ENABLE | EV_ONESHOT,
            NOTE_NSECONDS,
            duration.count(),
            &timerContext);

        if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1)
        {
            throw std::runtime_error("Timer::stop, kevent failed, " + lastErrorMessage());
        }

        context = &timerContext;
        dispatcher->getCurrentContext()->interruptProcedure = [&] {
            assert(dispatcher != nullptr);
            assert(context != nullptr);
            OperationContext *timerContext = static_cast<OperationContext *>(context);
            if (!timerContext->interrupted)
            {
                struct kevent event;
                EV_SET(&event, timer, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);

                if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1)
                {
                    throw std::runtime_error("Timer::stop, kevent failed, " + lastErrorMessage());
                }

                dispatcher->pushContext(timerContext->context);
                timerContext->interrupted = true;
            }
        };

        dispatcher->dispatch();
        dispatcher->getCurrentContext()->interruptProcedure = nullptr;
        assert(dispatcher != nullptr);
        assert(timerContext.context == dispatcher->getCurrentContext());
        assert(context == &timerContext);
        context = nullptr;
        timerContext.context = nullptr;
        dispatcher->pushTimer(timer);
        if (timerContext.interrupted)
        {
            throw InterruptedException();
        }
    }

} // namespace System
