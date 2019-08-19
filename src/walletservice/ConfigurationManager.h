// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "WalletServiceConfiguration.h"
#include "p2p/NetNodeConfig.h"

#include <CryptoTypes.h>

namespace PaymentService
{
    class ConfigurationManager
    {
      public:
        ConfigurationManager();

        bool init(int argc, char **argv);

        WalletServiceConfiguration serviceConfig;

        Crypto::Hash rpcSecret;
    };

} // namespace PaymentService