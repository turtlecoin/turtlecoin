// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.


#include "FormatTools.h"
#include <cstdio>
#include <ctime>
#include "CryptoNoteConfig.h"
#include "CryptoNoteCore/Core.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include <boost/format.hpp>

namespace Common {

//--------------------------------------------------------------------------------
std::string get_mining_speed(uint32_t hr) {
  if (hr>1e9) return (boost::format("%.2f GH/s") % (hr/1e9)).str();
  if (hr>1e6) return (boost::format("%.2f MH/s") % (hr/1e6)).str();
  if (hr>1e3) return (boost::format("%.2f KH/s") % (hr/1e3)).str();

  return (boost::format("%.0f H/s") % hr).str();
}

//--------------------------------------------------------------------------------
std::string get_sync_percentage(uint64_t height, uint64_t target_height) {
  /* Don't divide by zero */
  if (height == 0 || target_height == 0)
  {
    return "0.00";
  }

  /* So we don't have > 100% */
  if (height > target_height)
  {
      height = target_height;
  }

  float pc = 100.0f * height / target_height;

  if (height < target_height && pc > 99.99f) {
    pc = 99.99f; // to avoid 100% when not fully synced
  }

  return (boost::format("%.2f") % pc).str();
}

enum ForkStatus { UpToDate, ForkLater, ForkSoonReady, ForkSoonNotReady, OutOfDate };

ForkStatus get_fork_status(uint64_t height, std::vector<uint64_t> upgrade_heights, uint64_t supported_height)
{
    /* Allow fork heights to be empty */
    if (upgrade_heights.empty())
    {
        return UpToDate;
    }

    uint64_t next_fork = 0;

    for (auto upgrade : upgrade_heights)
    {
        /* We have hit an upgrade already that the user cannot support */
        if (height >= upgrade && supported_height < upgrade)
        {
            return OutOfDate;
        }

        /* Get the next fork height */
        if (upgrade > height)
        {
            next_fork = upgrade;
            break;
        }
    }

    float days = (next_fork - height) / CryptoNote::parameters::EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;

    /* Next fork in < 30 days away */
    if (days < 30)
    {
        /* Software doesn't support the next fork yet */
        if (supported_height < next_fork)
        {
            return ForkSoonNotReady;
        }
        else
        {
            return ForkSoonReady;
        }
    }

    if (height > next_fork)
    {
        return UpToDate;
    }

    return ForkLater;
}

std::string get_fork_time(uint64_t height, std::vector<uint64_t> upgrade_heights)
{
    uint64_t next_fork = 0;

    for (auto upgrade : upgrade_heights)
    {
        /* Get the next fork height */
        if (upgrade > height)
        {
            next_fork = upgrade;
            break;
        }
    }

    float days = (next_fork - height) / CryptoNote::parameters::EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;

    if (height == next_fork)
    {
        return " (forking now),";
    }
    else if (days < 1)
    {
        return (boost::format(" (next fork in %.1f hours),") % (days * 24)).str();
    }
    else
    {
        return (boost::format(" (next fork in %.1f days),") % days).str();
    }
}

std::string get_update_status(ForkStatus forkStatus, uint64_t height, std::vector<uint64_t> upgrade_heights)
{
    switch(forkStatus)
    {
        case UpToDate:
        case ForkLater:
        {
            return " up to date";
        }
        case ForkSoonReady:
        {
            return get_fork_time(height, upgrade_heights) + " up to date";
        }
        case ForkSoonNotReady:
        {
            return get_fork_time(height, upgrade_heights) + " update needed";
        }
        case OutOfDate:
        {
            return " out of date, likely forked";
        }
        default:
        {
            throw std::runtime_error("Unexpected case unhandled");
        }
    }
}

//--------------------------------------------------------------------------------
std::string get_upgrade_info(uint64_t supported_height, std::vector<uint64_t> upgrade_heights)
{
    for (auto upgrade : upgrade_heights)
    {
        if (upgrade > supported_height)
        {
            return "The network forked at height " + std::to_string(upgrade) + ", please update your software: " + CryptoNote::LATEST_VERSION_URL;
        }
    }

    /* This shouldnt happen */
    return std::string();
}

//--------------------------------------------------------------------------------
std::string get_status_string(CryptoNote::COMMAND_RPC_GET_INFO::response iresp) {
  std::stringstream ss;
  std::time_t uptime = std::time(nullptr) - iresp.start_time;
  auto forkStatus = get_fork_status(iresp.network_height, iresp.upgrade_heights, iresp.supported_height);

  ss << "Height: " << iresp.height << "/" << iresp.network_height
     << " (" << get_sync_percentage(iresp.height, iresp.network_height) << "%) "
     << "on " << (iresp.testnet ? "testnet, " : "mainnet, ")
     << (iresp.synced ? "synced, " : "syncing, ")
     << "net hash " << get_mining_speed(iresp.hashrate) << ", "
     << "v" << +iresp.major_version << ","
     << get_update_status(forkStatus, iresp.network_height, iresp.upgrade_heights)
     << ", " << iresp.outgoing_connections_count << "(out)+" << iresp.incoming_connections_count << "(in) connections, "
     << "uptime " << (unsigned int)floor(uptime / 60.0 / 60.0 / 24.0)
     << "d " << (unsigned int)floor(fmod((uptime / 60.0 / 60.0), 24.0))
     << "h " << (unsigned int)floor(fmod((uptime / 60.0), 60.0))
     << "m " << (unsigned int)fmod(uptime, 60.0) << "s";

  if (forkStatus == OutOfDate)
  {
      ss << std::endl << get_upgrade_info(iresp.supported_height, iresp.upgrade_heights);
  }

  return ss.str();
}

}
