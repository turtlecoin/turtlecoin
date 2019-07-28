// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "cryptonotecore/CryptoNoteBasic.h"

namespace CryptoNote
{
    struct IMinerHandler
    {
        virtual bool handle_block_found(BlockTemplate &b) = 0;

        virtual bool get_block_template(
            BlockTemplate &b,
            const AccountPublicAddress &adr,
            uint64_t &diffic,
            uint32_t &height,
            const BinaryArray &ex_nonce) = 0;

      protected:
        ~IMinerHandler() {};
    };
} // namespace CryptoNote
