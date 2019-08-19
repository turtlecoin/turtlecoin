// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoNote.h>

namespace CryptoNote
{
    class IMainChainStorage
    {
      public:
        virtual ~IMainChainStorage() {}

        virtual void pushBlock(const RawBlock &rawBlock) = 0;

        virtual void popBlock() = 0;

        virtual void rewindTo(uint32_t index) const = 0;

        virtual RawBlock getBlockByIndex(uint32_t index) const = 0;

        virtual uint32_t getBlockCount() const = 0;

        virtual void clear() = 0;
    };

} // namespace CryptoNote
