// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2019, The CyprusCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <rapidjson/document.h>
#include <config/CryptoNoteConfig.h>
#include <Logging/ILogger.h>
#include "Common/PathTools.h"
#include "Common/Util.h"

using namespace rapidjson;

namespace DaemonConfig {
  struct DaemonConfiguration
  {
    DaemonConfiguration()
    {
      std::stringstream logfile;
      logfile << CryptoNote::CRYPTONOTE_NAME << "d.log";

      dataDirectory = Tools::getDefaultDataDirectory();
      checkPoints = "default";
      logFile = logfile.str();
      logLevel = Logging::WARNING;
      dbMaxOpenFiles = CryptoNote::DATABASE_DEFAULT_MAX_OPEN_FILES;
      dbReadCacheSizeMB = CryptoNote::DATABASE_READ_BUFFER_MB_DEFAULT_SIZE;
      dbThreads = CryptoNote::DATABASE_DEFAULT_BACKGROUND_THREADS_COUNT;
      dbWriteBufferSizeMB = CryptoNote::DATABASE_WRITE_BUFFER_MB_DEFAULT_SIZE;
      rewindToHeight = 0;
      p2pInterface = "0.0.0.0";
      p2pPort = CryptoNote::P2P_DEFAULT_PORT;
      p2pExternalPort = 0;
      rpcInterface = "127.0.0.1";
      rpcPort = CryptoNote::RPC_DEFAULT_PORT;
      noConsole = false;
      enableBlockExplorer = false;
      localIp = false;
      hideMyPort = false;
      p2pResetPeerstate = false;
      help = false;
      version = false;
      osVersion = false;
      printGenesisTx = false;
      dumpConfig = false;
      useSqliteForLocalCaches = false;
      useRocksdbForLocalCaches = false;
      enableDbCompression = false;
      resync = false;
    }

    std::string dataDirectory;
    std::string logFile;
    std::string feeAddress;
    std::string rpcInterface;
    std::string p2pInterface;
    std::string checkPoints;

    std::vector<std::string> peers;
    std::vector<std::string> priorityNodes;
    std::vector<std::string> exclusiveNodes;
    std::vector<std::string> seedNodes;
    std::vector<std::string> enableCors;

    int logLevel;
    int feeAmount;
    int rpcPort;
    int p2pPort;
    int p2pExternalPort;
    int dbThreads;
    int dbMaxOpenFiles;
    int dbWriteBufferSizeMB;
    int dbReadCacheSizeMB;

    uint32_t rewindToHeight;

    bool noConsole;
    bool enableBlockExplorer;
    bool localIp;
    bool hideMyPort;
    bool resync;
    bool p2pResetPeerstate;

    std::string configFile;
    std::string outputFile;
    std::vector<std::string> genesisAwardAddresses;
    bool help;
    bool version;
    bool osVersion;
    bool printGenesisTx;
    bool dumpConfig;
    bool useSqliteForLocalCaches;
    bool useRocksdbForLocalCaches;
    bool enableDbCompression;
  };

  DaemonConfiguration initConfiguration(const char* path);

  bool updateConfigFormat(const std::string configFile, DaemonConfiguration& config);
  void handleSettings(int argc, char* argv[], DaemonConfiguration& config);
  void handleSettings(const std::string configFile, DaemonConfiguration& config);
  void asFile(const DaemonConfiguration& config, const std::string& filename);
  std::string asString(const DaemonConfiguration& config);
  Document asJSON(const DaemonConfiguration& config);
}