// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "BaseTests.h"

#include <System/Timer.h>
#include "WalletLegacy/WalletLegacy.h"
#include "WalletLegacyObserver.h"

using namespace Tests;
using namespace CryptoNote;

class WalletLegacyTests : public BaseTest
{

};


TEST_F(WalletLegacyTests, checkNetworkShutdown)
{
    auto networkCfg = TestNetworkBuilder(3, Topology::Star).
            setBlockchain("testnet_300").build();

    networkCfg[0].nodeType = NodeType::InProcess;
    network.addNodes(networkCfg);
    network.waitNodesReady();

    auto &daemon = network.getNode(0);

    {
        auto node = daemon.makeINode();
        WalletLegacy wallet(currency, *node);
        wallet.initAndGenerate("pass");

        WalletLegacyObserver observer;
        wallet.addObserver(&observer);

        std::error_code syncResult;
        ASSERT_TRUE(observer.m_syncResult.waitFor(std::chrono::seconds(10), syncResult));
        ASSERT_TRUE(!syncResult);

        // sync completed
        auto syncProgress = observer.getSyncProgress();

        network.getNode(1).stopDaemon();
        network.getNode(2).stopDaemon();

        System::Timer(dispatcher).sleep(std::chrono::seconds(10));

        // check that sync progress was not updated
        ASSERT_EQ(syncProgress, observer.getSyncProgress());
    }
}
