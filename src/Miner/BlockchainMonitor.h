// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoTypes.h"

#include <System/ContextGroup.h>
#include <System/Dispatcher.h>
#include <System/Event.h>

#include "Logging/LoggerRef.h"

class BlockchainMonitor
{
public:
    BlockchainMonitor(System::Dispatcher &dispatcher, const std::string &daemonHost, uint16_t daemonPort,
                      size_t pollingInterval, Logging::ILogger &logger);

    void waitBlockchainUpdate();

    void stop();

private:
    System::Dispatcher &m_dispatcher;
    std::string m_daemonHost;
    uint16_t m_daemonPort;
    size_t m_pollingInterval;
    bool m_stopped;
    System::Event m_httpEvent;
    System::ContextGroup m_sleepingContext;

    Logging::LoggerRef m_logger;

    Crypto::Hash requestLastBlockHash();
};
