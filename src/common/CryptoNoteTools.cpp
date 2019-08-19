// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "CryptoNoteTools.h"

using namespace CryptoNote;

void CryptoNote::getBinaryArrayHash(const BinaryArray &binaryArray, Crypto::Hash &hash)
{
    cn_fast_hash(binaryArray.data(), binaryArray.size(), hash);
}

Crypto::Hash CryptoNote::getBinaryArrayHash(const BinaryArray &binaryArray)
{
    Crypto::Hash hash;
    getBinaryArrayHash(binaryArray, hash);
    return hash;
}
