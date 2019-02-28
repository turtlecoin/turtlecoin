// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once
#include "CryptoNoteBasicImpl.h"
#include <Logging/LoggerRef.h>
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
