// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <gtest/gtest.h>
#include <System/ContextGroupTimeout.h>
#include <System/InterruptedException.h>

using namespace System;

class ContextGroupTimeoutTest : public testing::Test {
public:
  ContextGroupTimeoutTest() : contextGroup(dispatcher), timer(dispatcher) {
  }

  Dispatcher dispatcher;
  ContextGroup contextGroup;
  Timer timer;
};

TEST_F(ContextGroupTimeoutTest, timeoutHappens) {
  auto begin = std::chrono::high_resolution_clock::now();
  ContextGroupTimeout groupTimeout(dispatcher, contextGroup, std::chrono::milliseconds(100));
  contextGroup.spawn([&] {
    EXPECT_THROW(Timer(dispatcher).sleep(std::chrono::milliseconds(200)), InterruptedException);
  });
  contextGroup.wait();
  ASSERT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin).count(), 50);
  ASSERT_TRUE((std::chrono::high_resolution_clock::now() - begin) < std::chrono::milliseconds(150));
}

TEST_F(ContextGroupTimeoutTest, timeoutSkipped) {
  auto begin = std::chrono::high_resolution_clock::now();
  {
    ContextGroupTimeout op(dispatcher, contextGroup, std::chrono::milliseconds(200));
    contextGroup.spawn([&] {
      EXPECT_NO_THROW(Timer(dispatcher).sleep(std::chrono::milliseconds(100)));
    });
    contextGroup.wait();
  }
  ASSERT_TRUE((std::chrono::high_resolution_clock::now() - begin) > std::chrono::milliseconds(50));
  ASSERT_TRUE((std::chrono::high_resolution_clock::now() - begin) < std::chrono::milliseconds(150));
}

TEST_F(ContextGroupTimeoutTest, noOperation) {
  ContextGroupTimeout op(dispatcher, contextGroup, std::chrono::milliseconds(100));
}
