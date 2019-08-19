// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>

template<typename T> class ThreadSafeDeque
{
  public:
    ThreadSafeDeque(): m_shouldStop(false) {}

    ThreadSafeDeque(bool startStopped): m_shouldStop(startStopped) {}

    /* Move constructor */
    ThreadSafeDeque(ThreadSafeDeque &&old)
    {
        *this = std::move(old);
    }

    ThreadSafeDeque &operator=(ThreadSafeDeque &&old)
    {
        /* Stop anything running */
        stop();

        m_deque = std::move(old.m_deque);

        return *this;
    }

    /* Add the items to the end of the queue. They are added in the order
       of the iterators passed in, so once the operation has completed,
       the item pointed to by end will be at the end of the queue. */
    template<typename Iterator> bool push_back_n(Iterator begin, Iterator end)
    {
        /* Acquire the lock */
        std::unique_lock<std::mutex> lock(m_mutex);

        /* Stopping, don't push data */
        if (m_shouldStop)
        {
            return false;
        }

        for (auto it = begin; it < end; it++)
        {
            m_deque.push_back(*it);
        }

        /* Unlock the mutex before notifying, so it doesn't block after
           waking up */
        lock.unlock();

        /* Notify the consumer that we have some data */
        m_haveData.notify_all();

        return true;
    }

    /* Add an item to the end of the queue */
    bool push_back(T item)
    {
        /* Acquire the lock */
        std::unique_lock<std::mutex> lock(m_mutex);

        /* Stopping, don't push data */
        if (m_shouldStop)
        {
            return false;
        }

        /* Add the item to the front of the queue */
        m_deque.push_back(item);

        /* Unlock the mutex before notifying, so it doesn't block after
           waking up */
        lock.unlock();

        /* Notify the consumer that we have some data */
        m_haveData.notify_all();

        return true;
    }

    /* Delete the front item from the queue */
    void pop_front()
    {
        /* Acquire the lock */
        std::unique_lock<std::mutex> lock(m_mutex);

        /* Whilst we could allow deleting from an empty queue, i.e, waiting
           for an item, then removing it, this could cause us to be stuck
           waiting on data to arrive when the queue is empty. We can't
           really return without removing the item. We could return a bool
           saying if we completed it, but then the user has no real way to
           force a removal short of running it in a while loop.
           Instead, if we just force the queue to have to have data in,
           we can make sure a removal always succeeds. */
        if (m_deque.empty())
        {
            throw std::runtime_error("Cannot remove from an empty queue!");
        }

        /* Remove the first item from the queue */
        m_deque.pop_front();

        /* Unlock the mutex before notifying, so it doesn't block after
           waking up */
        lock.unlock();

        m_consumedData.notify_all();
    }

    /* Removes numElements from the start of the queue */
    void pop_front_n(size_t numElements)
    {
        /* Acquire the lock */
        std::unique_lock<std::mutex> lock(m_mutex);

        /* Whilst we could allow deleting from an empty queue, i.e, waiting
           for an item, then removing it, this could cause us to be stuck
           waiting on data to arrive when the queue is empty. We can't
           really return without removing the item. We could return a bool
           saying if we completed it, but then the user has no real way to
           force a removal short of running it in a while loop.
           Instead, if we just force the queue to have to have data in,
           we can make sure a removal always succeeds. */
        if (m_deque.empty())
        {
            throw std::runtime_error("Cannot remove from an empty queue!");
        }

        if (m_deque.size() < numElements)
        {
            throw std::runtime_error("Cannot remove more elements than are stored!");
        }

        while (numElements > 0)
        {
            /* Remove first item from queue */
            m_deque.pop_front();
            numElements--;
        }

        /* Unlock the mutex before notifying, so it doesn't block after
           waking up */
        lock.unlock();

        m_consumedData.notify_all();
    }

    /* Take an item from the front of the queue, and do NOT remove it */
    T front()
    {
        return getFirstItem(false);
    }

    /* Take and remove an item from the front of the queue */
    T front_and_remove()
    {
        return getFirstItem(true);
    }

    std::vector<T> front_n_and_remove(const size_t numElements)
    {
        /* Acquire the lock */
        std::unique_lock<std::mutex> lock(m_mutex);

        std::vector<T> results;

        if (m_deque.size() == 0)
        {
            return results;
        }

        if (m_deque.size() <= numElements)
        {
            results.resize(m_deque.size());
            std::copy(m_deque.begin(), m_deque.end(), results.begin());
            m_deque.clear();
        }
        else
        {
            results.resize(numElements);
            std::copy(m_deque.begin(), m_deque.begin() + numElements, results.begin());
            m_deque.erase(m_deque.begin(), m_deque.begin() + numElements);
        }

        return results;
    }

    /* Stop the queue if something is waiting on it, so we don't block
       whilst closing */
    void stop()
    {
        /* Make sure the queue knows to return */
        m_shouldStop = true;

        /* Wake up anything waiting on data */
        m_haveData.notify_all();

        /* Make sure not to call .unlock() on the mutex here - it's
           undefined behaviour if it isn't locked. */

        m_consumedData.notify_all();
    }

    void start()
    {
        m_shouldStop = false;
    }

    size_t size() const
    {
        /* Acquire the lock (I'm not sure if this is actually needed here,
           but hey) */
        std::unique_lock<std::mutex> lock(m_mutex);

        return m_deque.size();
    }

    /* Takes n elements, if available, starting at the head of the queue.
       Otherwise returns max available. Does not remove items from the queue. */
    std::vector<T> front_n(const size_t numElements) const
    {
        /* Acquire the lock */
        std::unique_lock<std::mutex> lock(m_mutex);

        std::vector<T> results;

        if (m_deque.size() <= numElements)
        {
            results.resize(m_deque.size());
            std::copy(m_deque.begin(), m_deque.end(), results.begin());
        }
        else
        {
            results.resize(numElements);
            std::copy(m_deque.begin(), m_deque.begin() + numElements, results.begin());
        }

        return results;
    }

    /* Takes n elements, if available, starting at the tail of the queue.
       Otherwise returns max available. Does not remove items from the queue. */
    std::vector<T> back_n(const size_t numElements) const
    {
        /* Aquire the lock */
        std::unique_lock<std::mutex> lock(m_mutex);

        std::vector<T> results;

        if (m_deque.size() <= numElements)
        {
            results.resize(m_deque.size());
            std::copy(m_deque.rbegin(), m_deque.rend(), results.begin());
        }
        else
        {
            results.resize(numElements);
            std::copy(m_deque.rbegin(), m_deque.rbegin() + numElements, results.begin());
        }

        return results;
    }

    void clear()
    {
        /* Acquire the lock */
        std::unique_lock<std::mutex> lock(m_mutex);

        m_deque.clear();
    }

    /* Recommended to implement a memoryUsage() method for your type, if not using
       a simple type, and pass to the below function instead.
       Otherwise, sizeof() is unlikely to be accurate. */
    size_t memoryUsage() const
    {
        /* Acquire the lock */
        std::unique_lock<std::mutex> lock(m_mutex);

        return m_deque.size() * sizeof(T) + sizeof(m_deque);
    }

    size_t memoryUsage(std::function<size_t(T)> memUsage) const
    {
        /* Acquire the lock */
        std::unique_lock<std::mutex> lock(m_mutex);

        return std::accumulate(
            m_deque.begin(), m_deque.end(), sizeof(m_deque), [&memUsage](const auto acc, const auto item) {
                return acc + memUsage(item);
            });
    }

  private:
    T getFirstItem(bool removeFromQueue)
    {
        /* Acquire the lock */
        std::unique_lock<std::mutex> lock(m_mutex);

        T item;

        /* Stopping, don't return data */
        if (m_shouldStop)
        {
            return item;
        }

        /* Wait for data to become available (releases the lock whilst
           it's not, so we don't block the producer) */
        m_haveData.wait(lock, [&] {
            /* Stopping, don't block */
            if (m_shouldStop)
            {
                return true;
            }

            return !m_deque.empty();
        });

        /* Stopping, don't return data */
        if (m_shouldStop)
        {
            return item;
        }

        /* Get the first item in the queue */
        item = m_deque.front();

        /* Remove the first item from the queue */
        if (removeFromQueue)
        {
            m_deque.pop_front();
        }

        /* Unlock the mutex before notifying, so it doesn't block after
           waking up */
        lock.unlock();

        m_consumedData.notify_all();

        /* Return the item */
        return item;
    }

  private:
    /* The deque data structure */
    std::deque<T> m_deque;

    /* The mutex, to ensure we have atomic access to the queue */
    mutable std::mutex m_mutex;

    /* Whether we have data or not */
    std::condition_variable m_haveData;

    /* Triggered when data is consumed */
    std::condition_variable m_consumedData;

    /* Whether we're stopping */
    std::atomic<bool> m_shouldStop;
};
