// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "EventWaiter.h"

void EventWaiter::notify() {
  std::unique_lock<std::mutex> lck(mtx);
  available = true;
  cv.notify_all();
}

void EventWaiter::wait() {
  std::unique_lock<std::mutex> lck(mtx);
  cv.wait(lck, [this]() { return available; });
  available = false;
}

bool EventWaiter::wait_for(const std::chrono::milliseconds& rel_time) {
  std::unique_lock<std::mutex> lck(mtx);
  auto result = cv.wait_for(lck, rel_time, [this]() { return available; });
  available = false;
  return result;
}
