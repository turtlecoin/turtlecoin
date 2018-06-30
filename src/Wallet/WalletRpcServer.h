// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma  once

#include <future>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include "WalletRpcServerCommandsDefinitions.h"
#include "WalletLegacy/WalletLegacy.h"
#include "Common/CommandLine.h"
#include "Rpc/HttpServer.h"

#include <Logging/LoggerRef.h>

namespace Tools
{
    /************************************************************************/
    /*                                                                      */
    /************************************************************************/
    class wallet_rpc_server : CryptoNote::HttpServer
    {
    public:

        wallet_rpc_server(
                System::Dispatcher &dispatcher,
                Logging::ILogger &log,
                CryptoNote::IWalletLegacy &w,
                CryptoNote::INode &n,
                CryptoNote::Currency &currency,
                const std::string &walletFilename);


        static void init_options(boost::program_options::options_description &desc);

        bool init(const boost::program_options::variables_map &vm);

        bool run();

        void send_stop_signal();

        static const command_line::arg_descriptor<uint16_t> arg_rpc_bind_port;
        static const command_line::arg_descriptor<std::string> arg_rpc_bind_ip;
        static const command_line::arg_descriptor<std::string> arg_rpc_password;
        static const command_line::arg_descriptor<bool> arg_rpc_legacy_security;

    private:

        virtual void
        processRequest(const CryptoNote::HttpRequest &request, CryptoNote::HttpResponse &response) override;

        //json_rpc
        bool on_getbalance(const wallet_rpc::COMMAND_RPC_GET_BALANCE::request &req,
                           wallet_rpc::COMMAND_RPC_GET_BALANCE::response &res);

        bool on_transfer(const wallet_rpc::COMMAND_RPC_TRANSFER::request &req,
                         wallet_rpc::COMMAND_RPC_TRANSFER::response &res);

        bool on_store(const wallet_rpc::COMMAND_RPC_STORE::request &req, wallet_rpc::COMMAND_RPC_STORE::response &res);

        bool
        on_stop_wallet(const wallet_rpc::COMMAND_RPC_STOP::request &req, wallet_rpc::COMMAND_RPC_STOP::response &res);

        bool on_get_payments(const wallet_rpc::COMMAND_RPC_GET_PAYMENTS::request &req,
                             wallet_rpc::COMMAND_RPC_GET_PAYMENTS::response &res);

        bool on_get_transfers(const wallet_rpc::COMMAND_RPC_GET_TRANSFERS::request &req,
                              wallet_rpc::COMMAND_RPC_GET_TRANSFERS::response &res);

        bool on_get_height(const wallet_rpc::COMMAND_RPC_GET_HEIGHT::request &req,
                           wallet_rpc::COMMAND_RPC_GET_HEIGHT::response &res);

        bool on_reset(const wallet_rpc::COMMAND_RPC_RESET::request &req, wallet_rpc::COMMAND_RPC_RESET::response &res);

        bool on_reset_from(const wallet_rpc::COMMAND_RPC_RESET_FROM::request &req,
                           wallet_rpc::COMMAND_RPC_RESET_FROM::response &res);

        bool handle_command_line(const boost::program_options::variables_map &vm);

        bool on_get_address(const wallet_rpc::COMMAND_RPC_GET_ADDRESS::request &req,
                            wallet_rpc::COMMAND_RPC_GET_ADDRESS::response &res);

        bool on_view_keys(const wallet_rpc::COMMAND_RPC_VIEW_KEYS::request &req,
                          wallet_rpc::COMMAND_RPC_VIEW_KEYS::response &res);

        Logging::LoggerRef logger;
        CryptoNote::IWalletLegacy &m_wallet;
        CryptoNote::INode &m_node;
        uint16_t m_port;
        bool m_allow_extended_rpc;
        bool m_legacy;
        std::string m_bind_ip;
        std::string m_password;
        CryptoNote::Currency &m_currency;
        const std::string m_walletFilename;

        System::Dispatcher &m_dispatcher;
        System::Event m_stopComplete;
    };
}
