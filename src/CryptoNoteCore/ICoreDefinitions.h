// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <vector>
#include <CryptoNote.h>
#include <CryptoTypes.h>
//#include <Serialization/ISerializer.h>

namespace CryptoNote
{

    struct BlockFullInfo : public RawBlock
    {
        Crypto::Hash block_id;
    };

    struct TransactionPrefixInfo
    {
        Crypto::Hash txHash;
        TransactionPrefix txPrefix;
    };

    struct BlockShortInfo
    {
        Crypto::Hash blockId;
        BinaryArray block;
        std::vector<TransactionPrefixInfo> txPrefixes;
    };

    void serialize(BlockFullInfo &, ISerializer &);

    void serialize(TransactionPrefixInfo &, ISerializer &);

    void serialize(BlockShortInfo &, ISerializer &);

}
