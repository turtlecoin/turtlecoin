// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <mutex>
#include <condition_variable>

template <typename T>
class ObservableValueBase {
public:
  ObservableValueBase(std::mutex& mtx, std::condition_variable& cv, const T defaultValue = T()) :
    m_mutex(mtx), m_cv(cv), m_value(defaultValue), m_updated(false) {
  }

  void set(T value) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_value = value;
    m_updated = true;
    m_cv.notify_all();
  }

  void increment() {
    std::lock_guard<std::mutex> lk(m_mutex);
    ++m_value;
    m_updated = true;
    m_cv.notify_all();
  }

  T get() {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_value;
  }

  bool waitFor(std::chrono::milliseconds ms, T& value) {
    std::unique_lock<std::mutex> lk(m_mutex);

    if (m_cv.wait_for(lk, ms, [this] { return m_updated; })) {
      value = m_value;
      m_updated = false;
      return true;
    }

    return false;
  }

  T wait() {
    std::unique_lock<std::mutex> lk(m_mutex);
    m_cv.wait(lk, [this] { return m_updated; });
    m_updated = false;
    return m_value;
  }

private:

  std::mutex& m_mutex;
  std::condition_variable& m_cv;

  T m_value;
  bool m_updated;
};
