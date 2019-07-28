// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

namespace Config
{
    class WalletConfig
    {
      public:
        /* Pretty self explanatory, this configures whether we process
           coinbase transactions in the wallet. Most wallets have not received
           coinbase transactions. */
        bool skipCoinbaseTransactions = true;
    };

    class DaemonConfig
    {
      public:
    };

    class GlobalConfig
    {
      public:
    };

    /* Global config, exposed as `config`.
       Example: `if (Config::config.wallet.scanCoinbaseTransactions)` */
    class Config
    {
      public:
        Config() {};

        /* Configuration for wallets */
        WalletConfig wallet;

        /* Configuration for the daemon */
        DaemonConfig daemon;

        /* Configuration for throughout the codebase */
        GlobalConfig global;
    };

    extern Config config;
} // namespace Config
