// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The Karai Developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2019, The CyprusCoin Developers
//
// Please see the included LICENSE file for more information.

#include <config/CliHeader.h>

#include "DaemonConfiguration.h"
#include "DaemonCommandsHandler.h"
#include "Common/ScopeExit.h"
#include "Common/SignalHandler.h"
#include "Common/StdOutputStream.h"
#include "Common/StdInputStream.h"
#include "Common/PathTools.h"
#include "Common/Util.h"
#include "Common/FileSystemShim.h"
#include "crypto/hash.h"
#include "Common/CryptoNoteTools.h"
#include "CryptoNoteCore/Core.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/DatabaseBlockchainCache.h"
#include "CryptoNoteCore/DatabaseBlockchainCacheFactory.h"
#include "CryptoNoteCore/MainChainStorage.h"
#include "CryptoNoteCore/MainChainStorageSqlite.h"
#include "CryptoNoteCore/MainChainStorageRocksdb.h"
#include "CryptoNoteCore/RocksDBWrapper.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "P2p/NetNode.h"
#include "P2p/NetNodeConfig.h"
#include "Rpc/RpcServer.h"
#include "Serialization/BinaryInputStreamSerializer.h"
#include "Serialization/BinaryOutputStreamSerializer.h"

#include <config/CryptoNoteCheckpoints.h>
#include <Logging/LoggerManager.h>

#include <Common/FileSystemShim.h>

#if defined(WIN32)
#include <crtdbg.h>
#include <io.h>
#else
#include <unistd.h>
#endif

using Common::JsonValue;
using namespace CryptoNote;
using namespace Logging;
using namespace DaemonConfig;

void print_genesis_tx_hex(const std::vector<std::string> rewardAddresses, const bool blockExplorerMode, std::shared_ptr<LoggerManager> logManager)
{
  std::vector<CryptoNote::AccountPublicAddress> rewardTargets;

  CryptoNote::CurrencyBuilder currencyBuilder(logManager);
  currencyBuilder.isBlockexplorer(blockExplorerMode);

  CryptoNote::Currency currency = currencyBuilder.currency();

  for (const auto& rewardAddress : rewardAddresses)
  {
    CryptoNote::AccountPublicAddress address;
    if (!currency.parseAccountAddressString(rewardAddress, address))
    {
      std::cout << "Failed to parse genesis reward address: " << rewardAddress << std::endl;
      return;
    }
    rewardTargets.emplace_back(std::move(address));
  }

  CryptoNote::Transaction transaction;

  if (rewardTargets.empty())
  {
    if (CryptoNote::parameters::GENESIS_BLOCK_REWARD > 0)
    {
      std::cout << "Error: Genesis Block Reward Addresses are not defined" << std::endl;
      return;
    }
    transaction = CryptoNote::CurrencyBuilder(logManager).generateGenesisTransaction();
  }
  else
  {
    transaction = CryptoNote::CurrencyBuilder(logManager).generateGenesisTransaction(rewardTargets);
  }

  std::string transactionHex = Common::toHex(CryptoNote::toBinaryArray(transaction));
  std::cout << getProjectCLIHeader() << std::endl << std::endl
    << "Replace the current GENESIS_COINBASE_TX_HEX line in src/config/CryptoNoteConfig.h with this one:" << std::endl
    << "const char GENESIS_COINBASE_TX_HEX[] = \"" << transactionHex << "\";" << std::endl;

  return;
}

JsonValue buildLoggerConfiguration(Level level, const std::string& logfile) {
  JsonValue loggerConfiguration(JsonValue::OBJECT);
  loggerConfiguration.insert("globalLevel", static_cast<int64_t>(level));

  JsonValue& cfgLoggers = loggerConfiguration.insert("loggers", JsonValue::ARRAY);

  JsonValue& fileLogger = cfgLoggers.pushBack(JsonValue::OBJECT);
  fileLogger.insert("type", "file");
  fileLogger.insert("filename", logfile);
  fileLogger.insert("level", static_cast<int64_t>(TRACE));

  JsonValue& consoleLogger = cfgLoggers.pushBack(JsonValue::OBJECT);
  consoleLogger.insert("type", "console");
  consoleLogger.insert("level", static_cast<int64_t>(TRACE));
  consoleLogger.insert("pattern", "%D %T %L ");

  return loggerConfiguration;
}

int main(int argc, char* argv[])
{
  fs::path temp = fs::path(argv[0]).filename();
  DaemonConfiguration config = initConfiguration(temp.string().c_str());

#ifdef WIN32
  _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

  const auto logManager = std::make_shared<LoggerManager>();
  LoggerRef logger(logManager, "daemon");

  // Initial loading of CLI parameters
  handleSettings(argc, argv, config);

  if (config.printGenesisTx) // Do we weant to generate the Genesis Tx?
  {
    print_genesis_tx_hex(config.genesisAwardAddresses, false, logManager);
    exit(0);
  }

  // If the user passed in the --config-file option, we need to handle that first
  if (!config.configFile.empty())
  {
    try
    {
      if(updateConfigFormat(config.configFile, config))
      {
          std::cout << std::endl << "Updating daemon configuration format..." << std::endl;
          asFile(config, config.configFile);
      }
    }
    catch(std::runtime_error& e)
    {
      std::cout << std::endl << "There was an error parsing the specified configuration file. Please check the file and try again:"
        << std::endl << e.what() << std::endl;
      exit(1);
    }
    catch(std::exception& e)
    {
      // pass
    }

    try
    {
      handleSettings(config.configFile, config);
    }
    catch (std::exception& e)
    {
      std::cout << std::endl << "There was an error parsing the specified configuration file. Please check the file and try again"
        << std::endl << e.what() << std::endl;
      exit(1);
    }
  }

  // Load in the CLI specified parameters again to overwrite anything from the config file
  handleSettings(argc, argv, config);

  if (config.dumpConfig)
  {
    std::cout << getProjectCLIHeader() << asString(config) << std::endl;
    exit(0);
  }
  else if (!config.outputFile.empty())
  {
    try {
      asFile(config, config.outputFile);
      std::cout << getProjectCLIHeader() << "Configuration saved to: " << config.outputFile << std::endl;
      exit(0);
    }
    catch (std::exception& e)
    {
      std::cout << getProjectCLIHeader() << "Could not save configuration to: " << config.outputFile
        << std::endl << e.what() << std::endl;
      exit(1);
    }
  }

  /* If we were given the resync arg, we're deleting everything */
  if (config.resync)
  {
    std::error_code ec;

    std::vector<std::string> removablePaths = {
      config.dataDirectory + "/" + CryptoNote::parameters::CRYPTONOTE_BLOCKS_FILENAME,
      config.dataDirectory + "/" + CryptoNote::parameters::CRYPTONOTE_BLOCKINDEXES_FILENAME,
      config.dataDirectory + "/" + CryptoNote::parameters::P2P_NET_DATA_FILENAME,
      config.dataDirectory + "/" + CryptoNote::parameters::CRYPTONOTE_BLOCKS_FILENAME + ".sqlite3",
      config.dataDirectory + "/" + CryptoNote::parameters::CRYPTONOTE_BLOCKS_FILENAME + ".rocksdb",
      config.dataDirectory + "/DB"
    };

    for (const auto path : removablePaths)
    {
      fs::remove_all(fs::path(path), ec);

      if (ec)
      {
        std::cout << "Could not delete data path: " << path << std::endl;
        exit(1);
      }
    }
  }

  try
  {
    fs::path cwdPath = fs::current_path();
    auto modulePath = cwdPath / temp;
    auto cfgLogFile = fs::path(config.logFile);

    if (cfgLogFile.empty()) {
      cfgLogFile = modulePath.replace_extension(".log");
    } else {
      if (!cfgLogFile.has_parent_path()) {
        cfgLogFile = modulePath.parent_path() / cfgLogFile;
      }
    }

    Level cfgLogLevel = static_cast<Level>(static_cast<int>(Logging::ERROR) + config.logLevel);

    // configure logging
    logManager->configure(buildLoggerConfiguration(cfgLogLevel, cfgLogFile.string()));

    logger(INFO, BRIGHT_GREEN) << getProjectCLIHeader() << std::endl;

    logger(INFO) << "Program Working Directory: " << cwdPath;

    //create objects and link them
    CryptoNote::CurrencyBuilder currencyBuilder(logManager);
    currencyBuilder.isBlockexplorer(config.enableBlockExplorer);

    try {
      currencyBuilder.currency();
    } catch (std::exception&) {
      std::cout << "GENESIS_COINBASE_TX_HEX constant has an incorrect value. Please launch: " << CryptoNote::CRYPTONOTE_NAME << "d --print-genesis-tx" << std::endl;
      return 1;
    }
    CryptoNote::Currency currency = currencyBuilder.currency();

    DataBaseConfig dbConfig;
    dbConfig.init(
      config.dataDirectory,
      config.dbThreads,
      config.dbMaxOpenFiles,
      config.dbWriteBufferSizeMB,
      config.dbReadCacheSizeMB,
      config.enableDbCompression
    );

    /* If we were told to rewind the blockchain to a certain height
       we will remove blocks until we're back at the height specified */
    if (config.rewindToHeight > 0)
    {
      logger(INFO) << "Rewinding blockchain to: " << config.rewindToHeight << std::endl;
      std::unique_ptr<IMainChainStorage> mainChainStorage;

      if (config.useSqliteForLocalCaches)
      {
        mainChainStorage = createSwappedMainChainStorageSqlite(config.dataDirectory, currency);
      }
      else if (config.useRocksdbForLocalCaches )
      {
        mainChainStorage = createSwappedMainChainStorageRocksdb(config.dataDirectory, currency, dbConfig);
      }
      else
      {
        mainChainStorage = createSwappedMainChainStorage(config.dataDirectory, currency);
      }

      mainChainStorage->rewindTo(config.rewindToHeight);

      logger(INFO) << "Blockchain rewound to: " << config.rewindToHeight << std::endl;
    }

    bool use_checkpoints = !config.checkPoints.empty();
    CryptoNote::Checkpoints checkpoints(logManager);

    if (use_checkpoints) {
      logger(INFO) << "Loading Checkpoints for faster initial sync...";
      if (config.checkPoints == "default")
      {
        for (const auto& cp : CryptoNote::CHECKPOINTS)
        {
          checkpoints.addCheckpoint(cp.index, cp.blockId);
        }
          logger(INFO) << "Loaded " << CryptoNote::CHECKPOINTS.size() << " default checkpoints";
      }
      else
      {
        bool results = checkpoints.loadCheckpointsFromFile(config.checkPoints);
        if (!results) {
          throw std::runtime_error("Failed to load checkpoints");
        }
      }
    }

    NetNodeConfig netNodeConfig;
    netNodeConfig.init(config.p2pInterface, config.p2pPort, config.p2pExternalPort, config.localIp,
      config.hideMyPort, config.dataDirectory, config.peers,
      config.exclusiveNodes, config.priorityNodes,
      config.seedNodes, config.p2pResetPeerstate);

    if (!Tools::create_directories_if_necessary(dbConfig.getDataDir()))
    {
      throw std::runtime_error("Can't create directory: " + dbConfig.getDataDir());
    }

    RocksDBWrapper database(logManager);
    database.init(dbConfig);
    Tools::ScopeExit dbShutdownOnExit([&database] () { database.shutdown(); });

    if (!DatabaseBlockchainCache::checkDBSchemeVersion(database, logManager))
    {
      dbShutdownOnExit.cancel();
      database.shutdown();

      database.destroy(dbConfig);

      database.init(dbConfig);
      dbShutdownOnExit.resume();
    }

    System::Dispatcher dispatcher;
    logger(INFO) << "Initializing core...";

    std::unique_ptr<IMainChainStorage> tmainChainStorage;
    if ( config.useSqliteForLocalCaches )
    {
      tmainChainStorage = createSwappedMainChainStorageSqlite(config.dataDirectory, currency);
    }
    else if ( config.useRocksdbForLocalCaches )
    {
      tmainChainStorage = createSwappedMainChainStorageRocksdb(config.dataDirectory, currency, dbConfig);
    }
    else
    {
      tmainChainStorage = createSwappedMainChainStorage(config.dataDirectory, currency);
    }

    CryptoNote::Core ccore(
      currency,
      logManager,
      std::move(checkpoints),
      dispatcher,
      std::unique_ptr<IBlockchainCacheFactory>(new DatabaseBlockchainCacheFactory(database, logger.getLogger())),
      std::move(tmainChainStorage)
    );

    ccore.load();
    logger(INFO) << "Core initialized OK";

    CryptoNote::CryptoNoteProtocolHandler cprotocol(currency, dispatcher, ccore, nullptr, logManager);
    CryptoNote::NodeServer p2psrv(dispatcher, cprotocol, logManager);
    CryptoNote::RpcServer rpcServer(dispatcher, logManager, ccore, p2psrv, cprotocol);

    cprotocol.set_p2p_endpoint(&p2psrv);
    DaemonCommandsHandler dch(ccore, p2psrv, logManager, &rpcServer);
    logger(INFO) << "Initializing p2p server...";
    if (!p2psrv.init(netNodeConfig))
    {
      logger(ERROR, BRIGHT_RED) << "Failed to initialize p2p server.";
      return 1;
    }

    logger(INFO) << "P2p server initialized OK";

    if (!config.noConsole)
    {
      dch.start_handling();
    }

    // Fire up the RPC Server
    logger(INFO) << "Starting core rpc server on address " << config.rpcInterface << ":" << config.rpcPort;
    rpcServer.setFeeAddress(config.feeAddress);
    rpcServer.setFeeAmount(config.feeAmount);
    rpcServer.enableCors(config.enableCors);
    rpcServer.start(config.rpcInterface, config.rpcPort);
    logger(INFO) << "Core rpc server started ok";

    Tools::SignalHandler::install([&dch] {
       dch.exit({});
       dch.stop_handling();
    });

    logger(INFO) << "Starting p2p net loop...";
    p2psrv.run();
    logger(INFO) << "p2p net loop stopped";

    dch.stop_handling();

    //stop components
    logger(INFO) << "Stopping core rpc server...";
    rpcServer.stop();

    //deinitialize components
    logger(INFO) << "Deinitializing p2p...";
    p2psrv.deinit();

    cprotocol.set_p2p_endpoint(nullptr);
    ccore.save();

  }
  catch (const std::exception& e)
  {
    logger(ERROR, BRIGHT_RED) << "Exception: " << e.what();
    return 1;
  }

  logger(INFO) << "Node stopped.";
  return 0;
}
