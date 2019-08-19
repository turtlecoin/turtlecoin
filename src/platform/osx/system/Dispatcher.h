// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <queue>
#include <stack>

namespace System
{
    struct NativeContextGroup;

    struct NativeContext
    {
        void *uctx;
        void *stackPtr;
        bool interrupted;
        bool inExecutionQueue;
        NativeContext *next;
        NativeContextGroup *group;
        NativeContext *groupPrev;
        NativeContext *groupNext;
        std::function<void()> procedure;
        std::function<void()> interruptProcedure;
    };

    struct NativeContextGroup
    {
        NativeContext *firstContext;
        NativeContext *lastContext;
        NativeContext *firstWaiter;
        NativeContext *lastWaiter;
    };

    struct OperationContext
    {
        NativeContext *context;
        bool interrupted;
    };

    class Dispatcher
    {
      public:
        Dispatcher();

        Dispatcher(const Dispatcher &) = delete;

        ~Dispatcher();

        Dispatcher &operator=(const Dispatcher &) = delete;

        void clear();

        void dispatch();

        NativeContext *getCurrentContext() const;

        void interrupt();

        void interrupt(NativeContext *context);

        bool interrupted();

        void pushContext(NativeContext *context);

        void remoteSpawn(std::function<void()> &&procedure);

        void yield();

        int getKqueue() const;

        NativeContext &getReusableContext();

        void pushReusableContext(NativeContext &);

        int getTimer();

        void pushTimer(int timer);

#ifdef __LP64__
        static const int SIZEOF_PTHREAD_MUTEX_T = 56 + sizeof(long);
#else

        static const int SIZEOF_PTHREAD_MUTEX_T = 40 + sizeof(long);

#endif

      private:
        void spawn(std::function<void()> &&procedure);

        int kqueue;

        int lastCreatedTimer;

        alignas(std::max_align_t) uint8_t mutex[SIZEOF_PTHREAD_MUTEX_T];

        std::atomic<bool> remoteSpawned;

        std::queue<std::function<void()>> remoteSpawningProcedures;

        std::stack<int> timers;

        NativeContext mainContext;

        NativeContextGroup contextGroup;

        NativeContext *currentContext;

        NativeContext *firstResumingContext;

        NativeContext *lastResumingContext;

        NativeContext *firstReusableContext;

        size_t runningContextCount;

        void contextProcedure(void *uctx);

        static void contextProcedureStatic(intptr_t context);
    };

} // namespace System
