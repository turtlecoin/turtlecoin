// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2020, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "common/ConsoleHandler.h"
#include "daemon/DaemonConfiguration.h"
#include "rpc/RpcServer.h"

#include <logging/LoggerManager.h>
#include <logging/LoggerRef.h>

namespace CryptoNote
{
    class Core;

    class NodeServer;
} // namespace CryptoNote

class DaemonCommandsHandler
{
  public:
    DaemonCommandsHandler(
        CryptoNote::Core &core,
        CryptoNote::NodeServer &srv,
        std::shared_ptr<Logging::LoggerManager> log,
        const std::string ip,
        const uint32_t port,
        const DaemonConfig::DaemonConfiguration &config);

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

    httplib::Client m_rpcServer;

    Logging::LoggerRef logger;

    DaemonConfig::DaemonConfiguration m_config;

    std::shared_ptr<Logging::LoggerManager> m_logManager;

    std::string get_commands_str();

    bool print_block_by_height(uint32_t height);

    bool print_block_by_hash(const std::string &arg);

    bool help(const std::vector<std::string> &args);

    bool print_pl(const std::vector<std::string> &args);

    bool print_cn(const std::vector<std::string> &args);

    bool set_log(const std::vector<std::string> &args);

    bool print_block(const std::vector<std::string> &args);

    bool print_tx(const std::vector<std::string> &args);

    bool print_pool(const std::vector<std::string> &args);

    bool print_pool_sh(const std::vector<std::string> &args);

    bool status(const std::vector<std::string> &args);
};
