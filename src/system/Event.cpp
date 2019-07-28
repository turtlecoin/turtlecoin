// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "Event.h"

#include <cassert>
#include <system/Dispatcher.h>
#include <system/InterruptedException.h>

namespace System
{
    namespace
    {
        struct EventWaiter
        {
            bool interrupted;
            EventWaiter *prev;
            EventWaiter *next;
            NativeContext *context;
        };

    } // namespace

    Event::Event(): dispatcher(nullptr) {}

    Event::Event(Dispatcher &dispatcher): dispatcher(&dispatcher), state(false), first(nullptr) {}

    Event::Event(Event &&other): dispatcher(other.dispatcher)
    {
        if (dispatcher != nullptr)
        {
            state = other.state;
            if (!state)
            {
                assert(other.first == nullptr);
                first = nullptr;
            }

            other.dispatcher = nullptr;
        }
    }

    Event::~Event()
    {
        assert(dispatcher == nullptr || state || first == nullptr);
    }

    Event &Event::operator=(Event &&other)
    {
        assert(dispatcher == nullptr || state || first == nullptr);
        dispatcher = other.dispatcher;
        if (dispatcher != nullptr)
        {
            state = other.state;
            if (!state)
            {
                assert(other.first == nullptr);
                first = nullptr;
            }

            other.dispatcher = nullptr;
        }

        return *this;
    }

    bool Event::get() const
    {
        assert(dispatcher != nullptr);
        return state;
    }

    void Event::clear()
    {
        assert(dispatcher != nullptr);
        if (state)
        {
            state = false;
            first = nullptr;
        }
    }

    void Event::set()
    {
        assert(dispatcher != nullptr);
        if (!state)
        {
            state = true;
            for (EventWaiter *waiter = static_cast<EventWaiter *>(first); waiter != nullptr; waiter = waiter->next)
            {
                waiter->context->interruptProcedure = nullptr;
                dispatcher->pushContext(waiter->context);
            }
        }
    }

    void Event::wait()
    {
        assert(dispatcher != nullptr);
        if (dispatcher->interrupted())
        {
            throw InterruptedException();
        }

        if (!state)
        {
            EventWaiter waiter = {false, nullptr, nullptr, dispatcher->getCurrentContext()};
            waiter.context->interruptProcedure = [&] {
                if (waiter.next != nullptr)
                {
                    assert(waiter.next->prev == &waiter);
                    waiter.next->prev = waiter.prev;
                }
                else
                {
                    assert(last == &waiter);
                    last = waiter.prev;
                }

                if (waiter.prev != nullptr)
                {
                    assert(waiter.prev->next == &waiter);
                    waiter.prev->next = waiter.next;
                }
                else
                {
                    assert(first == &waiter);
                    first = waiter.next;
                }

                assert(!waiter.interrupted);
                waiter.interrupted = true;
                dispatcher->pushContext(waiter.context);
            };

            if (first != nullptr)
            {
                static_cast<EventWaiter *>(last)->next = &waiter;
                waiter.prev = static_cast<EventWaiter *>(last);
            }
            else
            {
                first = &waiter;
            }

            last = &waiter;
            dispatcher->dispatch();
            assert(waiter.context == dispatcher->getCurrentContext());
            assert(waiter.context->interruptProcedure == nullptr);
            assert(dispatcher != nullptr);
            if (waiter.interrupted)
            {
                throw InterruptedException();
            }
        }
    }

} // namespace System
