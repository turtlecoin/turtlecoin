// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "BlockchainExplorerData.h"
#include "serialization/ISerializer.h"

namespace CryptoNote
{
    void serialize(TransactionOutputDetails &output, ISerializer &serializer);

    void serialize(TransactionOutputReferenceDetails &outputReference, ISerializer &serializer);

    void serialize(BaseInputDetails &inputBase, ISerializer &serializer);

    void serialize(KeyInputDetails &inputToKey, ISerializer &serializer);

    void serialize(TransactionInputDetails &input, ISerializer &serializer);

    void serialize(TransactionExtraDetails &extra, ISerializer &serializer);

    void serialize(TransactionDetails &transaction, ISerializer &serializer);

    void serialize(BlockDetails &block, ISerializer &serializer);

} // namespace CryptoNote
