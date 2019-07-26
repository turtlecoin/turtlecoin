// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <unordered_set>

#include <HTTP/HttpRequest.h>
#include <HTTP/HttpResponse.h>

#include <System/ContextGroup.h>
#include <System/Dispatcher.h>
#include <System/TcpListener.h>
#include <System/TcpConnection.h>
#include <System/Event.h>

#include <Logging/LoggerRef.h>

namespace CryptoNote
{

    class HttpServer
    {

    public:

        HttpServer(System::Dispatcher &dispatcher, std::shared_ptr<Logging::ILogger> log);

        void start(const std::string &address, uint16_t port);

        void stop();

        virtual void processRequest(const HttpRequest &request, HttpResponse &response) = 0;

    protected:

        System::Dispatcher &m_dispatcher;

    private:

        void acceptLoop();

        void connectionHandler(System::TcpConnection &&conn);

        System::ContextGroup workingContextGroup;
        Logging::LoggerRef logger;
        System::TcpListener m_listener;
        std::unordered_set<System::TcpConnection *> m_connections;
    };

}
