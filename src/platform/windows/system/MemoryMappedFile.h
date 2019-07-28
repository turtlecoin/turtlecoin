// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>
#include <string>
#include <system_error>

namespace System
{
    class MemoryMappedFile
    {
      public:
        MemoryMappedFile();

        ~MemoryMappedFile();

        void create(const std::string &path, uint64_t size, bool overwrite, std::error_code &ec);

        void create(const std::string &path, uint64_t size, bool overwrite);

        void open(const std::string &path, std::error_code &ec);

        void open(const std::string &path);

        void close(std::error_code &ec);

        void close();

        const std::string &path() const;

        uint64_t size() const;

        const uint8_t *data() const;

        uint8_t *data();

        bool isOpened() const;

        void rename(const std::string &newPath, std::error_code &ec);

        void rename(const std::string &newPath);

        void flush(uint8_t *data, uint64_t size, std::error_code &ec);

        void flush(uint8_t *data, uint64_t size);

        void swap(MemoryMappedFile &other);

      private:
        void *m_fileHandle;

        void *m_mappingHandle;

        std::string m_path;

        uint64_t m_size;

        uint8_t *m_data;
    };

} // namespace System
