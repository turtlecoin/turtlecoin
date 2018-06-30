// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

namespace CryptoNote
{
    class IBlockchainStorageObserver
    {
    public:
        virtual ~IBlockchainStorageObserver()
        {
        }

        virtual void blockchainUpdated() = 0;
    };
}
