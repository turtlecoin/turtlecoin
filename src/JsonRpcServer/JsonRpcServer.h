// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <system_error>

#include <System/Dispatcher.h>
#include <System/Event.h>
#include "Logging/ILogger.h"
#include "Logging/LoggerRef.h"
#include "Rpc/HttpServer.h"
#include "WalletService/ConfigurationManager.h"

namespace CryptoNote
{
    class HttpResponse;

    class HttpRequest;
}

namespace Common
{
    class JsonValue;
}

namespace System
{
    class TcpConnection;
}

namespace CryptoNote
{

    class JsonRpcServer : HttpServer
    {
    public:
        JsonRpcServer(System::Dispatcher &sys, System::Event &stopEvent, std::shared_ptr<Logging::ILogger> loggerGroup,
                      PaymentService::ConfigurationManager &config);

        JsonRpcServer(const JsonRpcServer &) = delete;

        void start(const std::string &bindAddress, uint16_t bindPort);

    protected:
        static void makeErrorResponse(const std::error_code &ec, Common::JsonValue &resp);

        static void makeMethodNotFoundResponse(Common::JsonValue &resp);

        static void makeInvalidPasswordResponse(Common::JsonValue &resp);

        static void makeGenericErrorReponse(Common::JsonValue &resp, const char *what, int errorCode = -32001);

        static void fillJsonResponse(const Common::JsonValue &v, Common::JsonValue &resp);

        static void prepareJsonResponse(const Common::JsonValue &req, Common::JsonValue &resp);

        static void makeJsonParsingErrorResponse(Common::JsonValue &resp);

        virtual void processJsonRpcRequest(const Common::JsonValue &req, Common::JsonValue &resp) = 0;

        PaymentService::ConfigurationManager &config;

    private:
        // HttpServer
        virtual void
        processRequest(const CryptoNote::HttpRequest &request, CryptoNote::HttpResponse &response) override;

        System::Event &stopEvent;
        Logging::LoggerRef logger;
    };

} //namespace CryptoNote
