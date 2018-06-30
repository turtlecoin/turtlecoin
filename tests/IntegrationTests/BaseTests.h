// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <gtest/gtest.h>
#include <future>

#include <Logging/ConsoleLogger.h>
#include <System/Dispatcher.h>
#include "CryptoNoteCore/Currency.h"

#include "../IntegrationTestLib/TestNetwork.h"

namespace Tests
{

    class BaseTest : public testing::Test
    {
    public:

        BaseTest() :
                currency(CryptoNote::CurrencyBuilder(logger).testnet(true).currency()),
                network(dispatcher, currency)
        {
        }

    protected:

        virtual void TearDown() override
        {
            network.shutdown();
        }

        System::Dispatcher &getDispatcher()
        {
            return dispatcher;
        }

        System::Dispatcher dispatcher;
        Logging::ConsoleLogger logger;
        CryptoNote::Currency currency;
        TestNetwork network;
    };

}
