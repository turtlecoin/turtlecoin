// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "ConfigurationManager.h"
#include "PaymentServiceConfiguration.h"

#include "Logging/ConsoleLogger.h"
#include "Logging/LoggerGroup.h"
#include "Logging/StreamLogger.h"

#include "PaymentGate/NodeFactory.h"
#include "PaymentGate/WalletService.h"

class PaymentGateService
{
public:
    PaymentGateService();

    bool init(int argc, char **argv);

    const PaymentService::ConfigurationManager &getConfig() const
    { return config; }

    PaymentService::WalletConfiguration getWalletConfig() const;

    const CryptoNote::Currency getCurrency();

    void run();

    void stop();

    Logging::ILogger &getLogger()
    { return logger; }

private:

    void runInProcess(Logging::LoggerRef &log);

    void runRpcProxy(Logging::LoggerRef &log);

    void runWalletService(const CryptoNote::Currency &currency, CryptoNote::INode &node);

    System::Dispatcher *dispatcher;
    System::Event *stopEvent;
    PaymentService::ConfigurationManager config;
    PaymentService::WalletService *service;
    CryptoNote::CurrencyBuilder currencyBuilder;

    Logging::LoggerGroup logger;
    std::ofstream fileStream;
    Logging::StreamLogger fileLogger;
    Logging::ConsoleLogger consoleLogger;
};
