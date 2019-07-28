// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "TcpConnector.h"

#include "Dispatcher.h"
#include "ErrorMessage.h"
#include "TcpConnection.h"

#include <cassert>
#include <fcntl.h>
#include <netdb.h>
#include <sys/errno.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <system/InterruptedException.h>
#include <system/Ipv4Address.h>
#include <unistd.h>

namespace System
{
    namespace
    {
        struct ConnectorContext : public OperationContext
        {
            int connection;
        };

    } // namespace

    TcpConnector::TcpConnector(): dispatcher(nullptr) {}

    TcpConnector::TcpConnector(Dispatcher &dispatcher): dispatcher(&dispatcher), context(nullptr) {}

    TcpConnector::TcpConnector(TcpConnector &&other): dispatcher(other.dispatcher)
    {
        if (other.dispatcher != nullptr)
        {
            assert(other.context == nullptr);
            context = nullptr;
            other.dispatcher = nullptr;
        }
    }

    TcpConnector::~TcpConnector()
    {
        assert(dispatcher == nullptr || context == nullptr);
    }

    TcpConnector &TcpConnector::operator=(TcpConnector &&other)
    {
        dispatcher = other.dispatcher;
        if (other.dispatcher != nullptr)
        {
            assert(other.context == nullptr);
            context = nullptr;
            other.dispatcher = nullptr;
        }

        return *this;
    }

    TcpConnection TcpConnector::connect(const Ipv4Address &address, uint16_t port)
    {
        assert(dispatcher != nullptr);
        assert(context == nullptr);
        if (dispatcher->interrupted())
        {
            throw InterruptedException();
        }

        std::string message;
        int connection = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (connection == -1)
        {
            message = "socket failed, " + lastErrorMessage();
        }
        else
        {
            sockaddr_in bindAddress;
            bindAddress.sin_family = AF_INET;
            bindAddress.sin_port = 0;
            bindAddress.sin_addr.s_addr = INADDR_ANY;
            if (bind(connection, reinterpret_cast<sockaddr *>(&bindAddress), sizeof bindAddress) != 0)
            {
                message = "bind failed, " + lastErrorMessage();
            }
            else
            {
                int flags = fcntl(connection, F_GETFL, 0);
                if (flags == -1 || fcntl(connection, F_SETFL, flags | O_NONBLOCK) == -1)
                {
                    message = "fcntl failed, " + lastErrorMessage();
                }
                else
                {
                    sockaddr_in addressData;
                    addressData.sin_family = AF_INET;
                    addressData.sin_port = htons(port);
                    addressData.sin_addr.s_addr = htonl(address.getValue());
                    int result = ::connect(connection, reinterpret_cast<sockaddr *>(&addressData), sizeof addressData);
                    if (result == -1)
                    {
                        if (errno == EINPROGRESS)
                        {
                            ConnectorContext connectorContext;
                            connectorContext.context = dispatcher->getCurrentContext();
                            connectorContext.interrupted = false;
                            connectorContext.connection = connection;

                            struct kevent event;
                            EV_SET(&event, connection, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, &connectorContext);
                            if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1)
                            {
                                message = "kevent failed, " + lastErrorMessage();
                            }
                            else
                            {
                                context = &connectorContext;
                                dispatcher->getCurrentContext()->interruptProcedure = [&] {
                                    assert(dispatcher != nullptr);
                                    assert(context != nullptr);
                                    ConnectorContext *connectorContext = static_cast<ConnectorContext *>(context);
                                    if (!connectorContext->interrupted)
                                    {
                                        if (close(connectorContext->connection) == -1)
                                        {
                                            throw std::runtime_error(
                                                "TcpListener::stop, close failed, " + lastErrorMessage());
                                        }

                                        dispatcher->pushContext(connectorContext->context);
                                        connectorContext->interrupted = true;
                                    }
                                };

                                dispatcher->dispatch();
                                dispatcher->getCurrentContext()->interruptProcedure = nullptr;
                                assert(dispatcher != nullptr);
                                assert(connectorContext.context == dispatcher->getCurrentContext());
                                assert(context == &connectorContext);
                                context = nullptr;
                                connectorContext.context = nullptr;
                                if (connectorContext.interrupted)
                                {
                                    throw InterruptedException();
                                }

                                struct kevent event;
                                EV_SET(&event, connection, EVFILT_WRITE, EV_ADD | EV_DISABLE, 0, 0, NULL);

                                if (kevent(dispatcher->getKqueue(), &event, 1, NULL, 0, NULL) == -1)
                                {
                                    message = "kevent failed, " + lastErrorMessage();
                                }
                                else
                                {
                                    int retval = -1;
                                    socklen_t retValLen = sizeof(retval);
                                    int s = getsockopt(connection, SOL_SOCKET, SO_ERROR, &retval, &retValLen);
                                    if (s == -1)
                                    {
                                        message = "getsockopt failed, " + lastErrorMessage();
                                    }
                                    else
                                    {
                                        if (retval != 0)
                                        {
                                            message = "getsockopt failed, " + lastErrorMessage();
                                        }
                                        else
                                        {
                                            return TcpConnection(*dispatcher, connection);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        return TcpConnection(*dispatcher, connection);
                    }
                }
            }

            int result = close(connection);
            if (result)
            {
            }
            assert(result != -1);
            ;
        }

        throw std::runtime_error("TcpConnector::connect, " + message);
    }

} // namespace System
