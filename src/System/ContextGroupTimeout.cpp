// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "ContextGroupTimeout.h"
#include <System/InterruptedException.h>

using namespace System;

ContextGroupTimeout::ContextGroupTimeout(Dispatcher &dispatcher, ContextGroup &contextGroup,
                                         std::chrono::nanoseconds timeout) :
        workingContextGroup(dispatcher),
        timeoutTimer(dispatcher)
{
    workingContextGroup.spawn([&, timeout]
                              {
                                  try
                                  {
                                      timeoutTimer.sleep(timeout);
                                      contextGroup.interrupt();
                                  } catch (InterruptedException &)
                                  {
                                  }
                              });
}
