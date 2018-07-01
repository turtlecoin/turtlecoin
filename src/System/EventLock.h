// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

namespace System
{

    class Event;

    class EventLock
    {
    public:
        explicit EventLock(Event &event);

        ~EventLock();

        EventLock &operator=(const EventLock &) = delete;

    private:
        Event &event;
    };

}
