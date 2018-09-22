// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <condition_variable>

#include <mutex>

#include <queue>

template <typename T>
class MultiThreadedDeque
{
    public:
        MultiThreadedDeque() :
            m_stop(false)
        {
        }

        void push_front(T item)
        {
            /* Aquire the lock */
            std::lock_guard<std::mutex> lock(m_mutex);

            /* Stopping, don't push data */
            if (m_stop)
            {
                return;
            }

            /* Add the item to the front of the queue */
            m_queue.push_front(item);

            /* Notify the consumer that we have some data */
            m_haveData.notify_one();

            /* Lock is automatically released when we go out of scope */
        }

        /* Take an item from the end of the queue */
        T pop_back()
        {
            /* Aquire the lock */
            std::unique_lock<std::mutex> lock(m_mutex);

            T item;

            /* Stopping, don't return data */
            if (m_stop)
            {
                return item;
            }
                
            /* Wait for data to become available (releases the lock whilst
               it's not, so we don't block the producer) */
            m_haveData.wait(lock, [&]
            { 
                /* Stopping, don't block */
                if (m_stop)
                {
                    return true;
                }

                return !m_queue.empty();
            });

            /* Stopping, don't return data */
            if (m_stop)
            {
                return item;
            }

            /* Get the last item in the queue */
            item = m_queue.back();

            /* Remove the last item from the queue */
            m_queue.pop_back();

            /* Return the item */
            return item;

            /* Lock is automatically released when we go out of scope */
        }

        /* Stop the queue if something is waiting on it, so we don't block
           whilst closing */
        void stop()
        {
            /* Make sure the queue knows to return */
            m_stop.store(true);

            /* Unlock the mutex so things stop waiting */
            m_mutex.unlock();

            /* Wake up anything waiting on data */
            m_haveData.notify_one();
        }

    private:
        /* The deque data structure */
        std::deque<T> m_queue;

        /* The mutex, to ensure we have atomic access to the queue */
        std::mutex m_mutex;

        /* Whether we have data or not */
        std::condition_variable m_haveData;

        /* Whether we're stopping */
        std::atomic<bool> m_stop;
};
