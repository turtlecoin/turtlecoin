// Copyright (c) 2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

/////////////////////////////////
#include <miner/BlockUtilities.h>
/////////////////////////////////

#include <common/CryptoNoteTools.h>
#include <common/Varint.h>
#include <serialization/CryptoNoteSerialization.h>
#include <serialization/SerializationTools.h>

std::vector<uint8_t> getParentBlockHashingBinaryArray(const CryptoNote::BlockTemplate &block, const bool headerOnly)
{
    return getParentBinaryArray(block, true, headerOnly);
}

std::vector<uint8_t> getParentBlockBinaryArray(const CryptoNote::BlockTemplate &block, const bool headerOnly)
{
    return getParentBinaryArray(block, false, headerOnly);
}

std::vector<uint8_t>
    getParentBinaryArray(const CryptoNote::BlockTemplate &block, const bool hashTransaction, const bool headerOnly)
{
    std::vector<uint8_t> binaryArray;

    auto serializer = makeParentBlockSerializer(block, hashTransaction, headerOnly);

    if (!toBinaryArray(serializer, binaryArray))
    {
        throw std::runtime_error("Can't serialize parent block");
    }

    return binaryArray;
}

std::vector<uint8_t> getBlockHashingBinaryArray(const CryptoNote::BlockTemplate &block)
{
    std::vector<uint8_t> blockHashingBinaryArray;

    if (!toBinaryArray(static_cast<const CryptoNote::BlockHeader &>(block), blockHashingBinaryArray))
    {
        throw std::runtime_error("Can't serialize BlockHeader");
    }

    std::vector<Crypto::Hash> transactionHashes;
    transactionHashes.reserve(block.transactionHashes.size() + 1);
    transactionHashes.push_back(getObjectHash(block.baseTransaction));
    transactionHashes.insert(transactionHashes.end(), block.transactionHashes.begin(), block.transactionHashes.end());

    Crypto::Hash treeHash;

    Crypto::tree_hash(transactionHashes.data(), transactionHashes.size(), treeHash);

    blockHashingBinaryArray.insert(blockHashingBinaryArray.end(), treeHash.data, treeHash.data + 32);

    auto transactionCount = Common::asBinaryArray(Tools::get_varint_data(block.transactionHashes.size() + 1));

    blockHashingBinaryArray.insert(blockHashingBinaryArray.end(), transactionCount.begin(), transactionCount.end());

    return blockHashingBinaryArray;
}

Crypto::Hash getBlockHash(const CryptoNote::BlockTemplate &block)
{
    auto blockHashingBinaryArray = getBlockHashingBinaryArray(block);

    if (block.majorVersion >= CryptoNote::BLOCK_MAJOR_VERSION_2)
    {
        const auto &parentBlock = getParentBlockHashingBinaryArray(block, false);
        blockHashingBinaryArray.insert(blockHashingBinaryArray.end(), parentBlock.begin(), parentBlock.end());
    }

    return CryptoNote::getObjectHash(blockHashingBinaryArray);
}

Crypto::Hash getMerkleRoot(const CryptoNote::BlockTemplate &block)
{
    return CryptoNote::getObjectHash(getBlockHashingBinaryArray(block));
}

Crypto::Hash getBlockLongHash(const CryptoNote::BlockTemplate &block)
{
    const std::vector<uint8_t> rawHashingBlock = block.majorVersion == CryptoNote::BLOCK_MAJOR_VERSION_1
                                                     ? getBlockHashingBinaryArray(block)
                                                     : getParentBlockHashingBinaryArray(block, true);

    Crypto::Hash hash;

    try
    {
        const auto hashingAlgorithm = CryptoNote::HASHING_ALGORITHMS_BY_BLOCK_VERSION.at(block.majorVersion);

        hashingAlgorithm(rawHashingBlock.data(), rawHashingBlock.size(), hash);

        return hash;
    }
    catch (const std::out_of_range &)
    {
        throw std::runtime_error("Unknown block major version.");
    }
}
