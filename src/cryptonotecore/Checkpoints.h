// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoNoteBasicImpl.h"

#include <logging/LoggerRef.h>
#include <map>

namespace CryptoNote
{
    class Checkpoints
    {
      public:
        Checkpoints(std::shared_ptr<Logging::ILogger> log);

        bool addCheckpoint(uint32_t index, const std::string &hash_str);

        bool loadCheckpointsFromFile(const std::string &fileName);

        bool isInCheckpointZone(uint32_t index) const;

        bool checkBlock(uint32_t index, const Crypto::Hash &h) const;

        bool checkBlock(uint32_t index, const Crypto::Hash &h, bool &isCheckpoint) const;

      private:
        std::map<uint32_t, Crypto::Hash> points;

        Logging::LoggerRef logger;
    };
} // namespace CryptoNote
