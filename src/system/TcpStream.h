// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <array>
#include <cstdint>
#include <streambuf>

namespace System
{
    class TcpConnection;

    class TcpStreambuf : public std::streambuf
    {
      public:
        explicit TcpStreambuf(TcpConnection &connection);

        TcpStreambuf(const TcpStreambuf &) = delete;

        ~TcpStreambuf();

        TcpStreambuf &operator=(const TcpStreambuf &) = delete;

      private:
        TcpConnection &connection;

        std::array<char, 4096> readBuf;

        std::array<uint8_t, 1024> writeBuf;

        std::streambuf::int_type overflow(std::streambuf::int_type ch) override;

        int sync() override;

        std::streambuf::int_type underflow() override;

        bool dumpBuffer(bool finalize);
    };

} // namespace System
