// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include "common/ConsoleHandler.h"

#include <logging/LoggerRef.h>
#include <logging/LoggerManager.h>
#include "rpc/RpcServer.h"
#include "rpc/CoreRpcServerCommandsDefinitions.h"
#include "rpc/JsonRpc.h"

namespace CryptoNote
{
    class Core;

    class NodeServer;
}

class DaemonCommandsHandler
{
public:
    DaemonCommandsHandler(CryptoNote::Core &core, CryptoNote::NodeServer &srv,
                          std::shared_ptr<Logging::LoggerManager> log, CryptoNote::RpcServer *prpc_server);

    bool start_handling()
    {
        m_consoleHandler.start();
        return true;
    }

    void stop_handling()
    {
        m_consoleHandler.stop();
    }

    bool exit(const std::vector<std::string> &args);

private:

    Common::ConsoleHandler m_consoleHandler;
    CryptoNote::Core &m_core;
    CryptoNote::NodeServer &m_srv;
    Logging::LoggerRef logger;
    std::shared_ptr<Logging::LoggerManager> m_logManager;
    CryptoNote::RpcServer *m_prpc_server;

    std::string get_commands_str();

    bool print_block_by_height(uint32_t height);

    bool print_block_by_hash(const std::string &arg);

    bool help(const std::vector<std::string> &args);

    bool print_pl(const std::vector<std::string> &args);

    bool show_hr(const std::vector<std::string> &args);

    bool hide_hr(const std::vector<std::string> &args);

    bool print_bc_outs(const std::vector<std::string> &args);

    bool print_cn(const std::vector<std::string> &args);

    bool print_bc(const std::vector<std::string> &args);

    bool print_bci(const std::vector<std::string> &args);

    bool set_log(const std::vector<std::string> &args);

    bool print_block(const std::vector<std::string> &args);

    bool print_tx(const std::vector<std::string> &args);

    bool print_pool(const std::vector<std::string> &args);

    bool print_pool_sh(const std::vector<std::string> &args);

    bool start_mining(const std::vector<std::string> &args);

    bool stop_mining(const std::vector<std::string> &args);

    bool status(const std::vector<std::string> &args);
};
