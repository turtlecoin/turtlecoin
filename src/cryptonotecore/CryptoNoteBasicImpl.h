// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "common/StringTools.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "cryptonotecore/CryptoNoteBasic.h"

namespace CryptoNote
{
    /************************************************************************/
    /* CryptoNote helper functions                                          */
    /************************************************************************/
    uint64_t getPenalizedAmount(uint64_t amount, size_t medianSize, size_t currentBlockSize);
} // namespace CryptoNote

bool parse_hash256(const std::string &str_hash, Crypto::Hash &hash);
