// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoNote.h"
#include "serialization/KVBinaryInputStreamSerializer.h"
#include "serialization/KVBinaryOutputStreamSerializer.h"

#include <common/MemoryInputStream.h>
#include <common/VectorOutputStream.h>

namespace System
{
    class TcpConnection;
}

namespace CryptoNote
{
    enum class LevinError : int32_t
    {
        OK = 0,
        ERROR_CONNECTION = -1,
        ERROR_CONNECTION_NOT_FOUND = -2,
        ERROR_CONNECTION_DESTROYED = -3,
        ERROR_CONNECTION_TIMEDOUT = -4,
        ERROR_CONNECTION_NO_DUPLEX_PROTOCOL = -5,
        ERROR_CONNECTION_HANDLER_NOT_DEFINED = -6,
        ERROR_FORMAT = -7,
    };

    const int32_t LEVIN_PROTOCOL_RETCODE_SUCCESS = 1;

    class LevinProtocol
    {
      public:
        LevinProtocol(System::TcpConnection &connection);

        template<typename Request, typename Response>
        bool invoke(uint32_t command, const Request &request, Response &response)
        {
            sendMessage(command, encode(request), true);

            Command cmd;
            readCommand(cmd);

            if (!cmd.isResponse)
            {
                return false;
            }

            return decode(cmd.buf, response);
        }

        template<typename Request> void notify(uint32_t command, const Request &request, int)
        {
            sendMessage(command, encode(request), false);
        }

        struct Command
        {
            uint32_t command;

            bool isNotify;

            bool isResponse;

            BinaryArray buf;

            bool needReply() const;
        };

        bool readCommand(Command &cmd);

        void sendMessage(uint32_t command, const BinaryArray &out, bool needResponse);

        void sendReply(uint32_t command, const BinaryArray &out, int32_t returnCode);

        template<typename T> static bool decode(const BinaryArray &buf, T &value)
        {
            try
            {
                Common::MemoryInputStream stream(buf.data(), buf.size());
                KVBinaryInputStreamSerializer serializer(stream);
                serialize(value, serializer);
            }
            catch (std::exception &)
            {
                return false;
            }

            return true;
        }

        template<typename T> static BinaryArray encode(const T &value)
        {
            BinaryArray result;
            KVBinaryOutputStreamSerializer serializer;
            serialize(const_cast<T &>(value), serializer);
            Common::VectorOutputStream stream(result);
            serializer.dump(stream);
            return result;
        }

      private:
        bool readStrict(uint8_t *ptr, size_t size);

        void writeStrict(const uint8_t *ptr, size_t size);

        System::TcpConnection &m_conn;
    };

} // namespace CryptoNote
