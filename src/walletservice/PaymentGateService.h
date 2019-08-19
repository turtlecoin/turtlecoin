// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "ConfigurationManager.h"
#include "logging/ConsoleLogger.h"
#include "logging/LoggerGroup.h"
#include "logging/StreamLogger.h"
#include "walletservice/NodeFactory.h"
#include "walletservice/WalletService.h"

#include <config/CliHeader.h>

class PaymentGateService
{
  public:
    PaymentGateService();

    bool init(int argc, char **argv);

    const PaymentService::ConfigurationManager &getConfig() const
    {
        return config;
    }

    PaymentService::WalletConfiguration getWalletConfig() const;

    const CryptoNote::Currency getCurrency();

    void run();

    void stop();

    std::shared_ptr<Logging::ILogger> getLogger()
    {
        return logger;
    }

  private:
    void runInProcess(Logging::LoggerRef &log);

    void runRpcProxy(Logging::LoggerRef &log);

    void runWalletService(const CryptoNote::Currency &currency, CryptoNote::INode &node);

    System::Dispatcher *dispatcher;

    System::Event *stopEvent;

    PaymentService::ConfigurationManager config;

    PaymentService::WalletService *service;

    std::shared_ptr<Logging::LoggerGroup> logger = std::make_shared<Logging::LoggerGroup>();

    std::shared_ptr<CryptoNote::CurrencyBuilder> currencyBuilder;

    std::ofstream fileStream;

    Logging::StreamLogger fileLogger;

    Logging::ConsoleLogger consoleLogger;
};
