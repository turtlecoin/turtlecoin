// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "Timer.h"

#include "Dispatcher.h"

#include <cassert>
#include <stdexcept>
#include <sys/epoll.h>
#include <sys/timerfd.h>
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

        if (duration.count() == 0)
        {
            dispatcher->yield();
        }
        else
        {
            timer = dispatcher->getTimer();

            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
            itimerspec expires;
            expires.it_interval.tv_nsec = expires.it_interval.tv_sec = 0;
            expires.it_value.tv_sec = seconds.count();
            expires.it_value.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(duration - seconds).count();
            timerfd_settime(timer, 0, &expires, NULL);

            ContextPair contextPair;
            OperationContext timerContext;
            timerContext.interrupted = false;
            timerContext.context = dispatcher->getCurrentContext();
            contextPair.writeContext = nullptr;
            contextPair.readContext = &timerContext;

            epoll_event timerEvent;
            timerEvent.events = EPOLLIN | EPOLLONESHOT;
            timerEvent.data.ptr = &contextPair;

            if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_MOD, timer, &timerEvent) == -1)
            {
                throw std::runtime_error("Timer::sleep, epoll_ctl failed, " + lastErrorMessage());
            }
            dispatcher->getCurrentContext()->interruptProcedure = [&]() {
                assert(dispatcher != nullptr);
                assert(context != nullptr);
                OperationContext *timerContext = static_cast<OperationContext *>(context);
                if (!timerContext->interrupted)
                {
                    uint64_t value = 0;
                    if (::read(timer, &value, sizeof value) == -1)
                    {
                        bool knownError = false;

                        if (errno == EAGAIN)
                        {
                            knownError = true;
                        }

                        if (errno == EWOULDBLOCK)
                        {
                            knownError = true;
                        }

                        if (knownError)
                        {
                            timerContext->interrupted = true;
                            dispatcher->pushContext(timerContext->context);
                        }
                        else
                        {
                            throw std::runtime_error(
                                "Timer::sleep, interrupt procedure, read failed, " + lastErrorMessage());
                        }
                    }
                    else
                    {
                        assert(value > 0);
                        dispatcher->pushContext(timerContext->context);
                    }

                    epoll_event timerEvent;
                    timerEvent.events = EPOLLONESHOT;
                    timerEvent.data.ptr = nullptr;

                    if (epoll_ctl(dispatcher->getEpoll(), EPOLL_CTL_MOD, timer, &timerEvent) == -1)
                    {
                        throw std::runtime_error(
                            "Timer::sleep, interrupt procedure, epoll_ctl failed, " + lastErrorMessage());
                    }
                }
            };

            context = &timerContext;
            dispatcher->dispatch();
            dispatcher->getCurrentContext()->interruptProcedure = nullptr;
            assert(dispatcher != nullptr);
            assert(timerContext.context == dispatcher->getCurrentContext());
            assert(contextPair.writeContext == nullptr);
            assert(context == &timerContext);
            context = nullptr;
            timerContext.context = nullptr;
            dispatcher->pushTimer(timer);
            if (timerContext.interrupted)
            {
                throw InterruptedException();
            }
        }
    }

} // namespace System
