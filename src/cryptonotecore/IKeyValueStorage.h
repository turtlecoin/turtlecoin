// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <vector>

namespace CryptoNote
{
    class WriteBatch;

    class ReadBatch;

    class IKeyValueStorage
    {
      public:
        virtual ~IKeyValueStorage() {}

        virtual bool insert(const WriteBatch &batch, bool sync = false) = 0;

        virtual void read(const ReadBatch &batch) const = 0;
    };
} // namespace CryptoNote
