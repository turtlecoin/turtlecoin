// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "PaymentServiceJsonRpcMessages.h"
#include "common/JsonValue.h"
#include "jsonrpcserver/JsonRpcServer.h"
#include "serialization/JsonInputValueSerializer.h"
#include "serialization/JsonOutputStreamSerializer.h"

#include <unordered_map>

namespace PaymentService
{
    class WalletService;

    class PaymentServiceJsonRpcServer : public CryptoNote::JsonRpcServer
    {
      public:
        PaymentServiceJsonRpcServer(
            System::Dispatcher &sys,
            System::Event &stopEvent,
            WalletService &service,
            std::shared_ptr<Logging::ILogger> loggerGroup,
            PaymentService::ConfigurationManager &config);

        PaymentServiceJsonRpcServer(const PaymentServiceJsonRpcServer &) = delete;

      protected:
        virtual void processJsonRpcRequest(const Common::JsonValue &req, Common::JsonValue &resp) override;

      private:
        WalletService &service;

        Logging::LoggerRef logger;

        typedef std::function<void(const Common::JsonValue &jsonRpcParams, Common::JsonValue &jsonResponse)>
            HandlerFunction;

        template<typename RequestType, typename ResponseType, typename RequestHandler>
        HandlerFunction jsonHandler(RequestHandler handler)
        {
            return [handler, this](const Common::JsonValue &jsonRpcParams, Common::JsonValue &jsonResponse) mutable {
                RequestType request;
                ResponseType response;

                try
                {
                    CryptoNote::JsonInputValueSerializer inputSerializer(
                        const_cast<Common::JsonValue &>(jsonRpcParams));
                    SerializeRequest(request, inputSerializer);
                }
                catch (std::exception &)
                {
                    makeGenericErrorReponse(jsonResponse, "Invalid Request", -32600);
                    return;
                }

                std::error_code ec = handler(request, response);
                if (ec)
                {
                    makeErrorResponse(ec, jsonResponse);
                    return;
                }

                CryptoNote::JsonOutputStreamSerializer outputSerializer;
                serialize(response, outputSerializer);
                fillJsonResponse(outputSerializer.getValue(), jsonResponse);
            };
        }

        template<typename RequestType>
        void SerializeRequest(RequestType &request, CryptoNote::JsonInputValueSerializer &inputSerializer)
        {
            serialize(request, inputSerializer);
        }

        void SerializeRequest(SendTransaction::Request &request, CryptoNote::JsonInputValueSerializer &inputSerializer)
        {
            request.serialize(inputSerializer, service);
        }

        void SerializeRequest(
            CreateDelayedTransaction::Request &request,
            CryptoNote::JsonInputValueSerializer &inputSerializer)
        {
            request.serialize(inputSerializer, service);
        }

        void SerializeRequest(
            SendFusionTransaction::Request &request,
            CryptoNote::JsonInputValueSerializer &inputSerializer)
        {
            request.serialize(inputSerializer, service);
        }

        std::unordered_map<std::string, HandlerFunction> handlers;

        std::error_code handleSave(const Save::Request &request, Save::Response &response);

        std::error_code handleExport(const Export::Request &request, Export::Response &response);

        std::error_code handleReset(const Reset::Request &request, Reset::Response &response);

        std::error_code handleCreateAddress(const CreateAddress::Request &request, CreateAddress::Response &response);

        std::error_code
            handleCreateAddressList(const CreateAddressList::Request &request, CreateAddressList::Response &response);

        std::error_code handleDeleteAddress(const DeleteAddress::Request &request, DeleteAddress::Response &response);

        std::error_code handleGetSpendKeys(const GetSpendKeys::Request &request, GetSpendKeys::Response &response);

        std::error_code handleGetBalance(const GetBalance::Request &request, GetBalance::Response &response);

        std::error_code
            handleGetBlockHashes(const GetBlockHashes::Request &request, GetBlockHashes::Response &response);

        std::error_code handleGetTransactionHashes(
            const GetTransactionHashes::Request &request,
            GetTransactionHashes::Response &response);

        std::error_code
            handleGetTransactions(const GetTransactions::Request &request, GetTransactions::Response &response);

        std::error_code handleGetUnconfirmedTransactionHashes(
            const GetUnconfirmedTransactionHashes::Request &request,
            GetUnconfirmedTransactionHashes::Response &response);

        std::error_code
            handleGetTransaction(const GetTransaction::Request &request, GetTransaction::Response &response);

        std::error_code handleSendTransaction(SendTransaction::Request &request, SendTransaction::Response &response);

        std::error_code handleCreateDelayedTransaction(
            CreateDelayedTransaction::Request &request,
            CreateDelayedTransaction::Response &response);

        std::error_code handleGetDelayedTransactionHashes(
            const GetDelayedTransactionHashes::Request &request,
            GetDelayedTransactionHashes::Response &response);

        std::error_code handleDeleteDelayedTransaction(
            const DeleteDelayedTransaction::Request &request,
            DeleteDelayedTransaction::Response &response);

        std::error_code handleSendDelayedTransaction(
            SendDelayedTransaction::Request &request,
            SendDelayedTransaction::Response &response);

        std::error_code handleGetViewKey(const GetViewKey::Request &request, GetViewKey::Response &response);

        std::error_code
            handleGetMnemonicSeed(const GetMnemonicSeed::Request &request, GetMnemonicSeed::Response &response);

        std::error_code handleGetStatus(const GetStatus::Request &request, GetStatus::Response &response);

        std::error_code handleGetAddresses(const GetAddresses::Request &request, GetAddresses::Response &response);

        std::error_code handleSendFusionTransaction(
            const SendFusionTransaction::Request &request,
            SendFusionTransaction::Response &response);

        std::error_code
            handleEstimateFusion(const EstimateFusion::Request &request, EstimateFusion::Response &response);

        std::error_code handleCreateIntegratedAddress(
            const CreateIntegratedAddress::Request &request,
            CreateIntegratedAddress::Response &response);

        std::error_code handleNodeFeeInfo(const NodeFeeInfo::Request &request, NodeFeeInfo::Response &response);
    };

} // namespace PaymentService
