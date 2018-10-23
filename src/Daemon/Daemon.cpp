// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2018, The Karai Developers
// 
// Please see the included LICENSE file for more information.

#include <fstream>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "DaemonCommandsHandler.h"

#include "Common/ScopeExit.h"
#include "Common/SignalHandler.h"
#include "Common/StdOutputStream.h"
#include "Common/StdInputStream.h"
#include "Common/PathTools.h"
#include "Common/Util.h"
#include "crypto/hash.h"
#include "CryptoNoteCore/CryptoNoteTools.h"
#include "CryptoNoteCore/Core.h"
#include "CryptoNoteCore/Currency.h"
#include "CryptoNoteCore/DatabaseBlockchainCache.h"
#include "CryptoNoteCore/DatabaseBlockchainCacheFactory.h"
#include "CryptoNoteCore/MainChainStorage.h"
#include "CryptoNoteCore/RocksDBWrapper.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandler.h"
#include "P2p/NetNode.h"
#include "P2p/NetNodeConfig.h"
#include "Rpc/RpcServer.h"
#include "Rpc/RpcServerConfig.h"
#include "Serialization/BinaryInputStreamSerializer.h"
#include "Serialization/BinaryOutputStreamSerializer.h"
#include "version.h"

#include <config/Ascii.h>
#include <config/CryptoNoteCheckpoints.h>

#include <Logging/LoggerManager.h>

#if defined(WIN32)
#include <crtdbg.h>
#include <io.h>
#else
#include <unistd.h>
#endif

using Common::JsonValue;
using namespace CryptoNote;
using namespace Logging;

namespace po = boost::program_options;

namespace
{
  const command_line::arg_descriptor<std::string> arg_config_file = {"config-file", "Specify configuration file", std::string(CryptoNote::CRYPTONOTE_NAME) + ".conf"};
  const command_line::arg_descriptor<bool>        arg_os_version  = {"os-version", ""};
  const command_line::arg_descriptor<std::string> arg_log_file    = {"log-file", "", ""};
  const command_line::arg_descriptor<int>         arg_log_level   = {"log-level", "", 2}; // info level
  const command_line::arg_descriptor<bool>        arg_console     = {"no-console", "Disable daemon console commands"};
  const command_line::arg_descriptor<bool>        arg_print_genesis_tx = { "print-genesis-tx", "Prints genesis' block tx hex to insert it to config and exits" };
  const command_line::arg_descriptor<std::vector<std::string>> arg_genesis_block_reward_address = { "genesis-block-reward-address", "" };
  const command_line::arg_descriptor<bool> arg_blockexplorer_on = {"enable-blockexplorer", "Enable blockchain explorer RPC", false};
  const command_line::arg_descriptor<std::vector<std::string>>        arg_enable_cors = { "enable-cors", "Adds header 'Access-Control-Allow-Origin' to the daemon's RPC responses. Uses the value as domain. Use * for all" };
  const command_line::arg_descriptor<bool>        arg_testnet_on  = {"testnet", "Used to deploy test nets. Checkpoints and hardcoded seeds are ignored, "
    "network id is changed. Use it with --data-dir flag. The wallet must be launched with --testnet flag.", false};
  const command_line::arg_descriptor<std::string> arg_load_checkpoints   = {"load-checkpoints", "<default|filename> Use builtin default checkpoints or checkpoint csv file for faster initial blockchain sync", "default"};
  const command_line::arg_descriptor<std::string> arg_set_fee_address = { "fee-address", "Sets fee address for light wallets that use the daemon.", "" };
  const command_line::arg_descriptor<int> arg_set_fee_amount = { "fee-amount", "Sets the fee amount for the light wallets that use the daemon.", 0 };
}

bool command_line_preprocessor(const boost::program_options::variables_map& vm, LoggerRef& logger);
void print_genesis_tx_hex(const po::variables_map& vm, LoggerManager& logManager) {
  std::vector<CryptoNote::AccountPublicAddress> targets;
  auto genesis_block_reward_addresses = command_line::get_arg(vm, arg_genesis_block_reward_address);
  CryptoNote::CurrencyBuilder currencyBuilder(logManager);
bool blockexplorer_mode = command_line::get_arg(vm, arg_blockexplorer_on);
currencyBuilder.isBlockexplorer(blockexplorer_mode);
  CryptoNote::Currency currency = currencyBuilder.currency();
  for (const auto& address_string : genesis_block_reward_addresses) {
     CryptoNote::AccountPublicAddress address;
    if (!currency.parseAccountAddressString(address_string, address)) {
      std::cout << "Failed to parse address: " << address_string << std::endl;
      return;
    }
    targets.emplace_back(std::move(address));
  }
  if (targets.empty()) {
    if (CryptoNote::parameters::GENESIS_BLOCK_REWARD > 0) {
      std::cout << "Error: genesis block reward addresses are not defined" << std::endl;
    } else {
  CryptoNote::Transaction tx = CryptoNote::CurrencyBuilder(logManager).generateGenesisTransaction();
  std::string tx_hex = Common::toHex(CryptoNote::toBinaryArray(tx));
  std::cout << "Replace the current GENESIS_COINBASE_TX_HEX line in src/config/CryptoNoteConfig.h with this one:" << std::endl;
  std::cout << "const char GENESIS_COINBASE_TX_HEX[] = \"" << tx_hex << "\";" << std::endl;
    }
  } else {
      CryptoNote::Transaction tx = CryptoNote::CurrencyBuilder(logManager).generateGenesisTransaction(targets);
      std::string tx_hex = Common::toHex(CryptoNote::toBinaryArray(tx));
      std::cout << "Replace the current GENESIS_COINBASE_TX_HEX line in src/config/CryptoNoteConfig.h with this one:" << std::endl;
      std::cout << "const char GENESIS_COINBASE_TX_HEX[] = \"" << tx_hex << "\";" << std::endl;
  }
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

/* Wait for input so users can read errors before the window closes if they
   launch from a GUI rather than a terminal */
void pause_for_input(int argc) {
  /* if they passed arguments they're probably in a terminal so the errors will
     stay visible */
  if (argc == 1) {
    #if defined(WIN32)
    if (_isatty(_fileno(stdout)) && _isatty(_fileno(stdin))) {
    #else
    if(isatty(fileno(stdout)) && isatty(fileno(stdin))) {
    #endif
      std::cout << "Press any key to close the program: ";
      getchar();
    }
  }
}

int main(int argc, char* argv[])
{

#ifdef WIN32
  _CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

  LoggerManager logManager;
  LoggerRef logger(logManager, "daemon");

  try {
    po::options_description desc_cmd_only("Command line options");
    po::options_description desc_cmd_sett("Command line options and settings options");

    command_line::add_arg(desc_cmd_only, command_line::arg_help);
    command_line::add_arg(desc_cmd_only, command_line::arg_version);
    command_line::add_arg(desc_cmd_only, arg_os_version);
    // tools::get_default_data_dir() can't be called during static initialization
    command_line::add_arg(desc_cmd_sett, command_line::arg_data_dir, Tools::getDefaultDataDirectory());
    command_line::add_arg(desc_cmd_only, arg_config_file);

    command_line::add_arg(desc_cmd_sett, arg_log_file);
    command_line::add_arg(desc_cmd_sett, arg_log_level);
    command_line::add_arg(desc_cmd_sett, arg_console);
    command_line::add_arg(desc_cmd_sett, arg_testnet_on);
    command_line::add_arg(desc_cmd_sett, arg_enable_cors);
    command_line::add_arg(desc_cmd_sett, arg_blockexplorer_on);
    command_line::add_arg(desc_cmd_sett, arg_print_genesis_tx);
    command_line::add_arg(desc_cmd_sett, arg_genesis_block_reward_address);
    command_line::add_arg(desc_cmd_sett, arg_load_checkpoints);
    command_line::add_arg(desc_cmd_sett, arg_set_fee_address);
    command_line::add_arg(desc_cmd_sett, arg_set_fee_amount);
    
    RpcServerConfig::initOptions(desc_cmd_sett);
    NetNodeConfig::initOptions(desc_cmd_sett);
    DataBaseConfig::initOptions(desc_cmd_sett);

    po::options_description desc_options("Allowed options");
    desc_options.add(desc_cmd_only).add(desc_cmd_sett);

    po::variables_map vm;
    boost::filesystem::path data_dir_path;
    bool r = command_line::handle_error_helper(desc_options, [&]()
    {
      po::store(po::parse_command_line(argc, argv, desc_options), vm);

      if (command_line::get_arg(vm, command_line::arg_help))
      {
        std::cout << CryptoNote::CRYPTONOTE_NAME << " v" << PROJECT_VERSION_LONG << ENDL << ENDL;
        std::cout << desc_options << std::endl;
        return false;
      }

      std::string config = command_line::get_arg(vm, arg_config_file);
      boost::filesystem::path config_path(config);

      boost::system::error_code ec;
      if (boost::filesystem::exists(config_path, ec)) {
        po::store(po::parse_config_file<char>(config_path.string<std::string>().c_str(), desc_cmd_sett), vm);
      }
      po::notify(vm);

      data_dir_path = command_line::get_arg(vm, command_line::arg_data_dir);

      if (command_line::get_arg(vm, arg_print_genesis_tx)) {
        print_genesis_tx_hex(vm, logManager);
        return false;
      }
      return true;
    });

    if (!r)
      return 1;

    auto modulePath = Common::NativePathToGeneric(argv[0]);
    auto cfgLogFile = Common::NativePathToGeneric(command_line::get_arg(vm, arg_log_file));

    if (cfgLogFile.empty()) {
      cfgLogFile = Common::ReplaceExtenstion(modulePath, ".log");
    } else {
      if (!Common::HasParentPath(cfgLogFile)) {
        cfgLogFile = Common::CombinePath(Common::GetPathDirectory(modulePath), cfgLogFile);
      }
    }

    Level cfgLogLevel = static_cast<Level>(static_cast<int>(Logging::ERROR) + command_line::get_arg(vm, arg_log_level));

    // configure logging
    logManager.configure(buildLoggerConfiguration(cfgLogLevel, cfgLogFile));

    /* Yay, ascii art ^__^ */
    logger(INFO, BRIGHT_GREEN) << asciiArt << ENDL;

    logger(INFO, BRIGHT_GREEN) << "Welcome to " << CryptoNote::CRYPTONOTE_NAME << " v" << PROJECT_VERSION_LONG;

    if (command_line_preprocessor(vm, logger)) {
      return 0;
    }

    logger(INFO) << "Module folder: " << argv[0];

    bool testnet_mode = command_line::get_arg(vm, arg_testnet_on);
    if (testnet_mode) {
      logger(INFO) << "Starting in testnet mode!";
    }

    //create objects and link them
    CryptoNote::CurrencyBuilder currencyBuilder(logManager);
    bool blockexplorer_mode = command_line::get_arg(vm, arg_blockexplorer_on);
    currencyBuilder.isBlockexplorer(blockexplorer_mode);
    currencyBuilder.testnet(testnet_mode);
    try {
      currencyBuilder.currency();
    } catch (std::exception&) {
      std::cout << "GENESIS_COINBASE_TX_HEX constant has an incorrect value. Please launch: " << CryptoNote::CRYPTONOTE_NAME << "d --" << arg_print_genesis_tx.name;
      return 1;
    }
    CryptoNote::Currency currency = currencyBuilder.currency();

    bool use_checkpoints = !command_line::get_arg(vm, arg_load_checkpoints).empty();

    CryptoNote::Checkpoints checkpoints(logManager);
    if (use_checkpoints && !testnet_mode) {
      logger(INFO) << "Loading Checkpoints for faster initial sync...";
      std::string checkpoints_file = command_line::get_arg(vm, arg_load_checkpoints);
      if (checkpoints_file == "default") {
        for (const auto& cp : CryptoNote::CHECKPOINTS) {
          checkpoints.addCheckpoint(cp.index, cp.blockId);
        }
        logger(INFO) << "Loaded " << CryptoNote::CHECKPOINTS.size() << " default checkpoints";
      } else {
        bool results = checkpoints.loadCheckpointsFromFile(checkpoints_file);
        if (!results) {
          throw std::runtime_error("Failed to load checkpoints");
        }
      }
    }

    NetNodeConfig netNodeConfig;
    netNodeConfig.init(vm);
    netNodeConfig.setTestnet(testnet_mode);

    RpcServerConfig rpcConfig;
    rpcConfig.init(vm);

    DataBaseConfig dbConfig;
    dbConfig.init(vm);

    if (dbConfig.isConfigFolderDefaulted()) {
      if (!Tools::create_directories_if_necessary(dbConfig.getDataDir())) {
        throw std::runtime_error("Can't create directory: " + dbConfig.getDataDir());
      }
    } else {
      if (!Tools::directoryExists(dbConfig.getDataDir())) {
        throw std::runtime_error("Directory does not exist: " + dbConfig.getDataDir());
      }
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
    CryptoNote::Core ccore(
      currency,
      logManager,
      std::move(checkpoints),
      dispatcher,
      std::unique_ptr<IBlockchainCacheFactory>(new DatabaseBlockchainCacheFactory(database, logger.getLogger())),
      createSwappedMainChainStorage(data_dir_path.string(), currency));

    ccore.load();
    logger(INFO) << "Core initialized OK";

    CryptoNote::CryptoNoteProtocolHandler cprotocol(currency, dispatcher, ccore, nullptr, logManager);
    CryptoNote::NodeServer p2psrv(dispatcher, cprotocol, logManager);
    CryptoNote::RpcServer rpcServer(dispatcher, logManager, ccore, p2psrv, cprotocol);

    cprotocol.set_p2p_endpoint(&p2psrv);
    //DaemonCommandsHandler dch(ccore, p2psrv, logManager);
    DaemonCommandsHandler dch(ccore, p2psrv, logManager, &rpcServer);
    logger(INFO) << "Initializing p2p server...";
    if (!p2psrv.init(netNodeConfig)) {
      logger(ERROR, BRIGHT_RED) << "Failed to initialize p2p server.";
      return 1;
    }

    logger(INFO) << "P2p server initialized OK";

    if (!command_line::has_arg(vm, arg_console)) {
      dch.start_handling();
    }

    logger(INFO) << "Starting core rpc server on address " << rpcConfig.getBindAddress();
    rpcServer.start(rpcConfig.bindIp, rpcConfig.bindPort);
    rpcServer.setFeeAddress(command_line::get_arg(vm, arg_set_fee_address));
    rpcServer.setFeeAmount(command_line::get_arg(vm, arg_set_fee_amount));
    rpcServer.enableCors(command_line::get_arg(vm, arg_enable_cors));
    logger(INFO) << "Core rpc server started ok";

    Tools::SignalHandler::install([&dch, &p2psrv] {
      dch.stop_handling();
      p2psrv.sendStopSignal();
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

  } catch (const std::exception& e) {
    logger(ERROR, BRIGHT_RED) << "Exception: " << e.what();
    return 1;
  }

  logger(INFO) << "Node stopped.";
  return 0;
}

bool command_line_preprocessor(const boost::program_options::variables_map &vm, LoggerRef &logger) {
  bool exit = false;

  if (command_line::get_arg(vm, command_line::arg_version)) {
    std::cout << CryptoNote::CRYPTONOTE_NAME << " v" << PROJECT_VERSION_LONG << ENDL;
    exit = true;
  }
  if (command_line::get_arg(vm, arg_os_version)) {
    std::cout << "OS: " << Tools::get_os_version_string() << ENDL;
    exit = true;
  }

  if (exit) {
    return true;
  }

  return false;
}
