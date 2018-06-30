// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <System/Dispatcher.h>
#include <System/ContextGroup.h>
#include <System/Event.h>
#include <System/InterruptedException.h>
#include <System/Ipv4Address.h>
#include <System/TcpConnection.h>
#include <System/TcpConnector.h>
#include <System/TcpListener.h>
#include <System/Timer.h>
#include <gtest/gtest.h>

using namespace System;

class TcpListenerTests : public testing::Test
{
public:
    TcpListenerTests() :
            event(dispatcher), listener(dispatcher, Ipv4Address("127.0.0.1"), 6666), contextGroup(dispatcher)
    {
    }

    Dispatcher dispatcher;
    Event event;
    TcpListener listener;
    ContextGroup contextGroup;
};

TEST_F(TcpListenerTests, tcpListener1)
{
    contextGroup.spawn([&]
                       {
                           TcpConnector connector(dispatcher);
                           connector.connect(Ipv4Address("127.0.0.1"), 6666);
                           event.set();
                       });

    listener.accept();
    event.wait();
}


TEST_F(TcpListenerTests, interruptListener)
{
    bool stopped = false;
    contextGroup.spawn([&]
                       {
                           try
                           {
                               listener.accept();
                           } catch (InterruptedException &)
                           {
                               stopped = true;
                           }
                       });
    contextGroup.interrupt();
    contextGroup.wait();

    ASSERT_TRUE(stopped);
}

TEST_F(TcpListenerTests, acceptAfterInterrupt)
{
    bool stopped = false;
    contextGroup.spawn([&]
                       {
                           try
                           {
                               listener.accept();
                           } catch (InterruptedException &)
                           {
                               stopped = true;
                           }
                       });
    contextGroup.interrupt();
    contextGroup.wait();

    ASSERT_TRUE(stopped);
    stopped = false;
    contextGroup.spawn([&]
                       {
                           Timer(dispatcher).sleep(std::chrono::milliseconds(1));
                           contextGroup.interrupt();
                       });
    contextGroup.spawn([&]
                       {
                           try
                           {
                               TcpConnector connector(dispatcher);
                               connector.connect(Ipv4Address("127.0.0.1"), 6666);
                           } catch (InterruptedException &)
                           {
                               stopped = true;
                           }
                       });
    contextGroup.spawn([&]
                       {
                           try
                           {
                               listener.accept();
                           } catch (InterruptedException &)
                           {
                               stopped = true;
                           }
                       });
    contextGroup.wait();
    ASSERT_FALSE(stopped);
}

TEST_F(TcpListenerTests, tcpListener3)
{
    bool stopped = false;
    contextGroup.spawn([&]
                       {
                           Timer(dispatcher).sleep(std::chrono::milliseconds(100));
                           contextGroup.interrupt();
                       });

    contextGroup.spawn([&]
                       {
                           try
                           {
                               listener.accept();
                           } catch (InterruptedException &)
                           {
                               stopped = true;
                           }
                       });

    contextGroup.wait();
    ASSERT_TRUE(stopped);
}
