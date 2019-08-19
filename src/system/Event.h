// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

namespace System
{
    class Dispatcher;

    class Event
    {
      public:
        Event();

        explicit Event(Dispatcher &dispatcher);

        Event(const Event &) = delete;

        Event(Event &&other);

        ~Event();

        Event &operator=(const Event &) = delete;

        Event &operator=(Event &&other);

        bool get() const;

        void clear();

        void set();

        void wait();

      private:
        Dispatcher *dispatcher;

        bool state;

        void *first;

        void *last;
    };

} // namespace System
