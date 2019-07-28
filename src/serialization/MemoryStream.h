// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <algorithm>
#include <common/IOutputStream.h>
#include <cstdint>
#include <cstring> // memcpy
#include <vector>

namespace CryptoNote
{
    class MemoryStream : public Common::IOutputStream
    {
      public:
        MemoryStream(): m_writePos(0) {}

        virtual uint64_t writeSome(const void *data, uint64_t size) override
        {
            if (size == 0)
            {
                return 0;
            }

            if (m_writePos + size > m_buffer.size())
            {
                m_buffer.resize(m_writePos + size);
            }

            memcpy(&m_buffer[m_writePos], data, size);
            m_writePos += size;
            return size;
        }

        uint64_t size()
        {
            return m_buffer.size();
        }

        const uint8_t *data()
        {
            return m_buffer.data();
        }

        void clear()
        {
            m_writePos = 0;
            m_buffer.resize(0);
        }

      private:
        uint64_t m_writePos;

        std::vector<uint8_t> m_buffer;
    };

} // namespace CryptoNote
