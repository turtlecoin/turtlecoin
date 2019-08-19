// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <system/Dispatcher.h>

namespace System
{
    class ContextGroup
    {
      public:
        explicit ContextGroup(Dispatcher &dispatcher);

        ContextGroup(const ContextGroup &) = delete;

        ContextGroup(ContextGroup &&other);

        ~ContextGroup();

        ContextGroup &operator=(const ContextGroup &) = delete;

        ContextGroup &operator=(ContextGroup &&other);

        void interrupt();

        void spawn(std::function<void()> &&procedure);

        void wait();

      private:
        Dispatcher *dispatcher;

        NativeContextGroup contextGroup;
    };

} // namespace System
