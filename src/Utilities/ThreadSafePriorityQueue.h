// Copyright (c) 2019, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <condition_variable>

#include <queue>

#include <mutex>

template<typename T, typename Comparison = std::less<T>>
class ThreadSafePriorityQueue
{
    public:
        ThreadSafePriorityQueue() :
            m_shouldStop(false)
        {
        }

        ThreadSafePriorityQueue(bool startStopped) :
            m_shouldStop(startStopped)
        {
        }

        /* Move constructor */
        ThreadSafePriorityQueue(ThreadSafePriorityQueue && old)
        {
            *this = std::move(old);
        }

        ThreadSafePriorityQueue& operator=(ThreadSafePriorityQueue && old)
        {
            /* Stop anything running */
            stop();

            m_priorityQueue = std::move(old.m_priorityQueue);

            return *this;
        }

        /* Add the items between [begin, end] to the queue */
        template<typename Iterator>
        bool push_n(Iterator begin, Iterator end)
        {
            /* Aquire the lock */
            std::unique_lock<std::mutex> lock(m_mutex);

            /* Stopping, don't push data */
            if (m_shouldStop)
            {
                return false;
            }

            for (auto it = begin; it < end; it++)
            {
                m_priorityQueue.push(*it);
            }

            /* Unlock the mutex before notifying, so it doesn't block after
               waking up */
            lock.unlock();

            /* Notify the consumer that we have some data */
            m_haveData.notify_all();

            return true;
        }

        /* Add an item to the end of the queue */
        bool push(T item)
        {
            /* Aquire the lock */
            std::unique_lock<std::mutex> lock(m_mutex);
			
            /* Stopping, don't push data */
            if (m_shouldStop)
            {
                return false;
            }

            m_priorityQueue.push(item);

            /* Unlock the mutex before notifying, so it doesn't block after
               waking up */
            lock.unlock();

            /* Notify the consumer that we have some data */
            m_haveData.notify_all();

            return true;
        }

        /* Delete the front item from the queue */
        void pop()
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
            if (m_priorityQueue.empty())
            {
                throw std::runtime_error("Cannot remove from an empty queue!");
            }

            /* Remove the first item from the queue */
            m_priorityQueue.pop();

            /* Unlock the mutex before notifying, so it doesn't block after
               waking up */
            lock.unlock();

            m_consumedData.notify_all();
        }

        /* Delete the front item from the queue, without aquiring the lock */
        void pop_unsafe()
        {
            m_priorityQueue.pop();
        }

        /* Get the top item, without aquiring the lock */
        T top_unsafe()
        {
            return m_priorityQueue.top();
        }

        /* Determine if the container is empty, without aquiring the lock */
        bool empty_unsafe()
        {
            return m_priorityQueue.empty();
        }

        /* Removes numElements from the start of the queue */
        void pop_n(size_t numElements)
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
            if (m_priorityQueue.empty())
            {
                throw std::runtime_error("Cannot remove from an empty queue!");
            }

            if (m_priorityQueue.size() < numElements)
            {
                throw std::runtime_error("Cannot remove more elements than are stored!");
            }

            while (numElements > 0)
            {
                /* Remove first item from queue */
                m_priorityQueue.pop_front();
                numElements--;
            }

            /* Unlock the mutex before notifying, so it doesn't block after
               waking up */
            lock.unlock();

            m_consumedData.notify_all();
        }

        /* Take an item from the front of the queue, and do NOT remove it */
        T top()
        {
            return getFirstItem(false);
        }

        /* Take and remove an item from the front of the queue */
        T top_and_remove()
        {
            return getFirstItem(true);
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
            /* Aquire the lock (I'm not sure if this is actually needed here,
               but hey) */
            std::unique_lock<std::mutex> lock(m_mutex);

            return m_priorityQueue.size();
        }

        void clear()
        {
            /* Aquire the lock */
            std::unique_lock<std::mutex> lock(m_mutex);

            while (!m_priorityQueue.empty())
            {
                m_priorityQueue.pop();
            }
        }

    private:
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

                return !m_priorityQueue.empty();
            });

            /* Stopping, don't return data */
            if (m_shouldStop)
            {
                return item;
            }

            /* Get the first item in the queue */
            item = m_priorityQueue.top();

            /* Remove the first item from the queue */
            if (removeFromQueue)
            {
                m_priorityQueue.pop();
            }

            /* Unlock the mutex before notifying, so it doesn't block after
               waking up */
            lock.unlock();

            m_consumedData.notify_all();
			
            /* Return the item */
            return item;
        }
        
        /* The deque data structure */
        std::priority_queue<T, std::vector<T>, Comparison> m_priorityQueue;

        /* The mutex, to ensure we have atomic access to the queue */
        mutable std::mutex m_mutex;

        /* Whether we have data or not */
        std::condition_variable m_haveData;

        /* Triggered when data is consumed */
        std::condition_variable m_consumedData;

        /* Whether we're stopping */
        std::atomic<bool> m_shouldStop;
};
