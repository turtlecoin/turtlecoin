// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IReadBatch.h"
#include "IWriteBatch.h"

#include <string>
#include <system_error>

namespace CryptoNote
{
    class IDataBase
    {
      public:
        virtual ~IDataBase() {}

        virtual std::error_code write(IWriteBatch &batch) = 0;

        virtual std::error_code read(IReadBatch &batch) = 0;

        virtual std::error_code readThreadSafe(IReadBatch &batch) = 0;

        virtual void recreate() = 0;
    };
} // namespace CryptoNote
