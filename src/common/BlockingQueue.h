// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

template<typename T, typename Container = std::deque<T>> class BlockingQueue
{
  public:
    typedef BlockingQueue<T, Container> ThisType;

    BlockingQueue(uint64_t maxSize = 1): m_maxSize(maxSize), m_closed(false) {}

    template<typename TT> bool push(TT &&v)
    {
        std::unique_lock<std::mutex> lk(m_mutex);

        while (!m_closed && m_queue.size() >= m_maxSize)
        {
            m_haveSpace.wait(lk);
        }

        if (m_closed)
        {
            return false;
        }

        m_queue.push_back(std::forward<TT>(v));
        m_haveData.notify_one();
        return true;
    }

    bool pop(T &v)
    {
        std::unique_lock<std::mutex> lk(m_mutex);

        while (m_queue.empty())
        {
            if (m_closed)
            {
                // all data has been processed, queue is closed
                return false;
            }
            m_haveData.wait(lk);
        }

        v = std::move(m_queue.front());
        m_queue.pop_front();

        // we can have several waiting threads to unblock
        if (m_closed && m_queue.empty())
        {
            m_haveSpace.notify_all();
        }
        else
        {
            m_haveSpace.notify_one();
        }

        return true;
    }

    void close(bool wait = false)
    {
        std::unique_lock<std::mutex> lk(m_mutex);
        m_closed = true;
        m_haveData.notify_all(); // wake up threads in pop()
        m_haveSpace.notify_all();

        if (wait)
        {
            while (!m_queue.empty())
            {
                m_haveSpace.wait(lk);
            }
        }
    }

    uint64_t size()
    {
        std::unique_lock<std::mutex> lk(m_mutex);
        return m_queue.size();
    }

    uint64_t capacity() const
    {
        return m_maxSize;
    }

  private:
    const uint64_t m_maxSize;

    Container m_queue;

    bool m_closed;

    std::mutex m_mutex;

    std::condition_variable m_haveData;

    std::condition_variable m_haveSpace;
};

template<typename QueueT> class GroupClose
{
  public:
    GroupClose(QueueT &queue, uint64_t groupSize): m_queue(queue), m_count(groupSize) {}

    void close()
    {
        if (m_count == 0)
        {
            return;
        }
        if (m_count.fetch_sub(1) == 1)
        {
            m_queue.close();
        }
    }

  private:
    std::atomic<uint64_t> m_count;

    QueueT &m_queue;
};
