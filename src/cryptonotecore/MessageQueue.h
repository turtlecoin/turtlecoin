// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IntrusiveLinkedList.h"
#include "system/Dispatcher.h"
#include "system/Event.h"
#include "system/InterruptedException.h"

#include <queue>

namespace CryptoNote
{
    template<class MessageType> class MessageQueue
    {
      public:
        MessageQueue(System::Dispatcher &dispatcher);

        const MessageType &front();

        void pop();

        void push(const MessageType &message);

        void stop();

      private:
        friend class IntrusiveLinkedList<MessageQueue<MessageType>>;

        typename IntrusiveLinkedList<MessageQueue<MessageType>>::hook &getHook();

        void wait();

        System::Dispatcher &dispatcher;

        std::queue<MessageType> messageQueue;

        System::Event event;

        bool stopped;

        typename IntrusiveLinkedList<MessageQueue<MessageType>>::hook hook;
    };

    template<class MessageQueueContainer, class MessageType> class MesageQueueGuard
    {
      public:
        MesageQueueGuard(MessageQueueContainer &container, MessageQueue<MessageType> &messageQueue):
            container(container),
            messageQueue(messageQueue)
        {
            container.addMessageQueue(messageQueue);
        }

        ~MesageQueueGuard()
        {
            container.removeMessageQueue(messageQueue);
        }

      private:
        MessageQueueContainer &container;

        MessageQueue<MessageType> &messageQueue;
    };

    template<class MessageType>
    MessageQueue<MessageType>::MessageQueue(System::Dispatcher &dispatch):
        dispatcher(dispatch),
        event(dispatch),
        stopped(false)
    {
    }

    template<class MessageType> void MessageQueue<MessageType>::wait()
    {
        if (messageQueue.empty())
        {
            if (stopped)
            {
                throw System::InterruptedException();
            }

            event.clear();
            while (!event.get())
            {
                event.wait();

                if (stopped)
                {
                    throw System::InterruptedException();
                }
            }
        }
    }

    template<class MessageType> const MessageType &MessageQueue<MessageType>::front()
    {
        wait();
        return messageQueue.front();
    }

    template<class MessageType> void MessageQueue<MessageType>::pop()
    {
        wait();
        messageQueue.pop();
    }

    template<class MessageType> void MessageQueue<MessageType>::push(const MessageType &message)
    {
        dispatcher.remoteSpawn([=]() mutable {
            messageQueue.push(std::move(message));
            event.set();
        });
    }

    template<class MessageType> void MessageQueue<MessageType>::stop()
    {
        stopped = true;
        event.set();
    }

    template<class MessageType>
    typename IntrusiveLinkedList<MessageQueue<MessageType>>::hook &MessageQueue<MessageType>::getHook()
    {
        return hook;
    }
} // namespace CryptoNote
