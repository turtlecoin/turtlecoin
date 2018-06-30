// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "RpcServerConfig.h"
#include "Common/CommandLine.h"
#include "CryptoNoteConfig.h"

namespace CryptoNote
{

    namespace
    {

        const std::string DEFAULT_RPC_IP = "127.0.0.1";
        const uint16_t DEFAULT_RPC_PORT = RPC_DEFAULT_PORT;

        const command_line::arg_descriptor<std::string> arg_rpc_bind_ip = {"rpc-bind-ip", "Interface for RPC service",
                                                                           DEFAULT_RPC_IP};
        const command_line::arg_descriptor<uint16_t> arg_rpc_bind_port = {"rpc-bind-port", "Port for RPC service",
                                                                          DEFAULT_RPC_PORT};
    }


    RpcServerConfig::RpcServerConfig() : bindIp(DEFAULT_RPC_IP), bindPort(DEFAULT_RPC_PORT)
    {
    }

    std::string RpcServerConfig::getBindAddress() const
    {
        return bindIp + ":" + std::to_string(bindPort);
    }

    void RpcServerConfig::initOptions(boost::program_options::options_description &desc)
    {
        command_line::add_arg(desc, arg_rpc_bind_ip);
        command_line::add_arg(desc, arg_rpc_bind_port);
    }

    void RpcServerConfig::init(const boost::program_options::variables_map &vm)
    {
        bindIp = command_line::get_arg(vm, arg_rpc_bind_ip);
        bindPort = command_line::get_arg(vm, arg_rpc_bind_port);
    }

}
