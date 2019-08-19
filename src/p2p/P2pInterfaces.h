// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoNote.h>
#include <cstdint>
#include <memory>

namespace CryptoNote
{
    struct P2pMessage
    {
        uint32_t type;
        BinaryArray data;
    };

    class IP2pConnection
    {
      public:
        virtual ~IP2pConnection();

        virtual void read(P2pMessage &message) = 0;

        virtual void write(const P2pMessage &message) = 0;

        virtual void stop() = 0;
    };

    class IP2pNode
    {
      public:
        virtual void stop() = 0;
    };

} // namespace CryptoNote
