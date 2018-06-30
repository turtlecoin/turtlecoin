// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "EventLock.h"
#include <System/Event.h>

namespace System
{

    EventLock::EventLock(Event &event) : event(event)
    {
        while (!event.get())
        {
            event.wait();
        }

        event.clear();
    }

    EventLock::~EventLock()
    {
        event.set();
    }

}
