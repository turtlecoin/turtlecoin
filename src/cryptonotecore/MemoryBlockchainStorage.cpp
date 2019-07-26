// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "MemoryBlockchainStorage.h"
#include <cassert>
#include "serialization/SerializationOverloads.h"

using namespace CryptoNote;

MemoryBlockchainStorage::MemoryBlockchainStorage(uint32_t reserveSize)
{
    blocks.reserve(reserveSize);
}

MemoryBlockchainStorage::~MemoryBlockchainStorage()
{
}

void MemoryBlockchainStorage::pushBlock(RawBlock &&rawBlock)
{
    blocks.push_back(rawBlock);
}

RawBlock MemoryBlockchainStorage::getBlockByIndex(uint32_t index) const
{
    assert(index < getBlockCount());
    return blocks[index];
}

uint32_t MemoryBlockchainStorage::getBlockCount() const
{
    return static_cast<uint32_t>(blocks.size());
}

//Returns MemoryBlockchainStorage with elements from [splitIndex, blocks.size() - 1].
//Original MemoryBlockchainStorage will contain elements from [0, splitIndex - 1].
std::unique_ptr<BlockchainStorage::IBlockchainStorageInternal>
MemoryBlockchainStorage::splitStorage(uint32_t splitIndex)
{
    assert(splitIndex > 0);
    assert(splitIndex < blocks.size());
    std::unique_ptr<MemoryBlockchainStorage> newStorage(new MemoryBlockchainStorage(splitIndex));
    std::move(blocks.begin() + splitIndex, blocks.end(), std::back_inserter(newStorage->blocks));
    blocks.resize(splitIndex);
    blocks.shrink_to_fit();
    return std::move(newStorage);
}
