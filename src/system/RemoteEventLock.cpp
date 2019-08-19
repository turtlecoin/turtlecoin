// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "RemoteEventLock.h"

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <system/Dispatcher.h>
#include <system/Event.h>

namespace System
{
    RemoteEventLock::RemoteEventLock(Dispatcher &dispatcher, Event &event): dispatcher(dispatcher), event(event)
    {
        std::mutex mutex;
        std::condition_variable condition;
        bool locked = false;

        dispatcher.remoteSpawn([&]() {
            while (!event.get())
            {
                event.wait();
            }

            event.clear();
            mutex.lock();
            locked = true;
            condition.notify_one();
            mutex.unlock();
        });

        std::unique_lock<std::mutex> lock(mutex);
        while (!locked)
        {
            condition.wait(lock);
        }
    }

    RemoteEventLock::~RemoteEventLock()
    {
        std::mutex mutex;
        std::condition_variable condition;
        bool locked = true;

        Event *eventPointer = &event;
        dispatcher.remoteSpawn([&]() {
            assert(!eventPointer->get());
            eventPointer->set();

            mutex.lock();
            locked = false;
            condition.notify_one();
            mutex.unlock();
        });

        std::unique_lock<std::mutex> lock(mutex);
        while (locked)
        {
            condition.wait(lock);
        }
    }

} // namespace System
