// Copyright (c) 2018-2019, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <condition_variable>

#include <mutex>

#include <queue>

template <typename T>
class ThreadSafeQueue
{
    public:
        ThreadSafeQueue() :
            m_shouldStop(false)
        {
        }

        ThreadSafeQueue(bool startStopped) :
            m_shouldStop(startStopped)
        {
        }

        bool push(T item)
        {
            /* Aquire the lock */
            std::unique_lock<std::mutex> lock(m_mutex);
			
            /* Stopping, don't push data */
            if (m_shouldStop)
            {
                return false;
            }

            /* Add the item to the front of the queue */
            m_queue.push(item);

            /* Unlock the mutex before notifying, so it doesn't block after
               waking up */
            lock.unlock();

            /* Notify the consumer that we have some data */
            m_haveData.notify_all();

            return true;
        }

        /* Delete the front item from the queue */
        void deleteFront()
        {
            /* Aquire the lock */
            std::unique_lock<std::mutex> lock(m_mutex);

            /* Whilst we could allow deleting from an empty queue, i.e, waiting
               for an item, then removing it, this could cause us to be stuck
               waiting on data to arrive when the queue is empty. We can't
               really return without removing the item. We could return a bool
               saying if we completed it, but then the user has no real way to
               force a removal short of running it in a while loop.
               Instead, if we just force the queue to have to have data in,
               we can make sure a removal always suceeds. */
            if (m_queue.empty())
            {
                throw std::runtime_error("Cannot remove from an empty queue!");
            }

            /* Remove the first item from the queue */
            m_queue.pop();

            /* Unlock the mutex before notifying, so it doesn't block after
               waking up */
            lock.unlock();

            m_consumedData.notify_all();
        }
        
        T getFirstItem(bool removeFromQueue)
        {
            /* Aquire the lock */
            std::unique_lock<std::mutex> lock(m_mutex);
			
            T item;

            /* Stopping, don't return data */
            if (m_shouldStop)
            {
                return item;
            }
                
            /* Wait for data to become available (releases the lock whilst
               it's not, so we don't block the producer) */
            m_haveData.wait(lock, [&]
            { 
                /* Stopping, don't block */
                if (m_shouldStop)
                {
                    return true;
                }

                return !m_queue.empty();
            });

            /* Stopping, don't return data */
            if (m_shouldStop)
            {
                return item;
            }

            /* Get the first item in the queue */
            item = m_queue.front();

            /* Remove the first item from the queue */
            if (removeFromQueue)
            {
                m_queue.pop();
            }

            /* Unlock the mutex before notifying, so it doesn't block after
               waking up */
            lock.unlock();

            m_consumedData.notify_all();
			
            /* Return the item */
            return item;
        }

        /* Take an item from the front of the queue, and do NOT remove it */
        T peek()
        {
            bool removeFromQueue = false;
            return getFirstItem(removeFromQueue);
        }

        /* Take and remove an item from the front of the queue */
        T pop()
        {
            bool removeFromQueue = true;
            return getFirstItem(removeFromQueue);
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

    private:
        /* The deque data structure */
        std::queue<T> m_queue;

        /* The mutex, to ensure we have atomic access to the queue */
        std::mutex m_mutex;

        /* Whether we have data or not */
        std::condition_variable m_haveData;

        /* Triggered when data is consumed */
        std::condition_variable m_consumedData;

        /* Whether we're stopping */
        std::atomic<bool> m_shouldStop;
};
