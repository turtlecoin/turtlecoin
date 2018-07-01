// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <string>
#include "Rpc/CoreRpcServerCommandsDefinitions.h"

namespace Common
{
    std::string get_mining_speed(uint32_t hr);

    std::string get_sync_percentage(uint64_t height, uint64_t target_height);

    std::string get_upgrade_time(uint64_t height, uint64_t upgrade_height);

    std::string get_status_string(CryptoNote::COMMAND_RPC_GET_INFO::response iresp);
}
