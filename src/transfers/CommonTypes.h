// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "INode.h"
#include "ITransaction.h"

#include <array>
#include <boost/optional.hpp>
#include <cstdint>
#include <memory>

namespace CryptoNote
{
    struct BlockchainInterval
    {
        uint32_t startHeight;
        std::vector<Crypto::Hash> blocks;
    };

    struct CompleteBlock
    {
        Crypto::Hash blockHash;
        boost::optional<CryptoNote::BlockTemplate> block;
        // first transaction is always coinbase
        std::list<std::shared_ptr<ITransactionReader>> transactions;
    };

} // namespace CryptoNote
