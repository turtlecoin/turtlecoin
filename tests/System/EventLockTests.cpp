// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <System/Context.h>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/EventLock.h>
#include <gtest/gtest.h>

using namespace System;

TEST(EventLockTests, eventLockIsLocking) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  bool done = false;
  Context<> context(dispatcher, [&]() {
    EventLock lock(event);
    done = true;
  });

  ASSERT_FALSE(done);
  dispatcher.yield();
  ASSERT_FALSE(done);
  event.set();
  dispatcher.yield();
  ASSERT_TRUE(done);
}

TEST(EventLockTests, eventLockIsNotLocking) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  event.set();
  bool done = false;
  Context<> context(dispatcher, [&]() {
    EventLock lock(event);
    done = true;
  });

  ASSERT_FALSE(done);
  dispatcher.yield();
  ASSERT_TRUE(done);
}

TEST(EventLockTests, eventLockIsUnlockOnlyOnce) {
  Dispatcher dispatcher;
  Event event(dispatcher);
  auto i = 0;
  Context<> context(dispatcher, [&]() {
    EventLock lock(event);
    i++;
    dispatcher.yield();
    i++;
  });

  Context<> contextSecond(dispatcher, [&]() {
    EventLock lock(event);
    i += 2;
    dispatcher.yield();
    i += 2;
  });
  
  event.set();
  dispatcher.yield();
  ASSERT_EQ(i, 1);
  dispatcher.yield();
  ASSERT_EQ(i, 2);
  dispatcher.yield();
  ASSERT_EQ(i, 4);
  dispatcher.yield();
  ASSERT_EQ(i, 6);
}
