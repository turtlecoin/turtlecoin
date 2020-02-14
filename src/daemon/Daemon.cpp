// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The Karai Developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2019, The CyprusCoin Developers
//
// Please see the included LICENSE file for more information.

#include "DaemonCommandsHandler.h"
#include "DaemonConfiguration.h"
#include "common/CryptoNoteTools.h"
#include "common/FileSystemShim.h"
#include "common/PathTools.h"
#include "common/ScopeExit.h"
#include "common/SignalHandler.h"
#include "common/StdInputStream.h"
#include "common/StdOutputStream.h"
#include "common/Util.h"
#include "crypto/hash.h"
#include "cryptonotecore/Core.h"
#include "cryptonotecore/Currency.h"
#include "cryptonotecore/DatabaseBlockchainCache.h"
#include "cryptonotecore/DatabaseBlockchainCacheFactory.h"
#include "cryptonotecore/RocksDBWrapper.h"
#include "cryptonoteprotocol/CryptoNoteProtocolHandler.h"
#include "p2p/NetNode.h"
#include "p2p/NetNodeConfig.h"
#include "rpc/RpcServer.h"
#include "serialization/BinaryInputStreamSerializer.h"
#include "serialization/BinaryOutputStreamSerializer.h"

#include <common/FileSystemShim.h>
#include <config/CliHeader.h>
#include <config/CryptoNoteCheckpoints.h>
#include <logging/LoggerManager.h>
#include <logger/Logger.h>

#if defined(WIN32)

#undef ERROR
#include <crtdbg.h>
#include <io.h>

#else
#include <unistd.h>
#endif

using Common::JsonValue;
using namespace CryptoNote;
using namespace Logging;
using namespace DaemonConfig;

void print_genesis_tx_hex(const bool blockExplorerMode, std::shared_ptr<LoggerManager> logManager)
{
    CryptoNote::CurrencyBuilder currencyBuilder(logManager);
    currencyBuilder.isBlockexplorer(blockExplorerMode);

    CryptoNote::Currency currency = currencyBuilder.currency();

    const auto transaction = CryptoNote::CurrencyBuilder(logManager).generateGenesisTransaction();

    std::string transactionHex = Common::toHex(CryptoNote::toBinaryArray(transaction));
    std::cout << getProjectCLIHeader() << std::endl
              << std::endl
              << "Replace the current GENESIS_COINBASE_TX_HEX line in src/config/CryptoNoteConfig.h with this one:"
              << std::endl
              << "const char GENESIS_COINBASE_TX_HEX[] = \"" << transactionHex << "\";" << std::endl;

    return;
}

JsonValue buildLoggerConfiguration(Level level, const std::string &logfile)
{
    JsonValue loggerConfiguration(JsonValue::OBJECT);
    loggerConfiguration.insert("globalLevel", static_cast<int64_t>(level));

    JsonValue &cfgLoggers = loggerConfiguration.insert("loggers", JsonValue::ARRAY);

    JsonValue &fileLogger = cfgLoggers.pushBack(JsonValue::OBJECT);
    fileLogger.insert("type", "file");
    fileLogger.insert("filename", logfile);
    fileLogger.insert("level", static_cast<int64_t>(TRACE));

    JsonValue &consoleLogger = cfgLoggers.pushBack(JsonValue::OBJECT);
    consoleLogger.insert("type", "console");
    consoleLogger.insert("level", static_cast<int64_t>(TRACE));
    consoleLogger.insert("pattern", "%D %T %L ");

    return loggerConfiguration;
}

int main(int argc, char *argv[])
{
    fs::path temp = fs::path(argv[0]).filename();
    DaemonConfiguration config = initConfiguration(temp.string().c_str());

#ifdef WIN32
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    const auto logManager = std::make_shared<LoggerManager>();
    LoggerRef logger(logManager, "daemon");

    // Initial loading of CLI parameters
    handleSettings(argc, argv, config);

    if (config.printGenesisTx) // Do we weant to generate the Genesis Tx?
    {
        print_genesis_tx_hex(false, logManager);
        exit(0);
    }

    // If the user passed in the --config-file option, we need to handle that first
    if (!config.configFile.empty())
    {
        try
        {
            if (updateConfigFormat(config.configFile, config))
            {
                std::cout << std::endl << "Updating daemon configuration format..." << std::endl;
                asFile(config, config.configFile);
            }
        }
        catch (std::runtime_error &e)
        {
            std::cout
                << std::endl
                << "There was an error parsing the specified configuration file. Please check the file and try again:"
                << std::endl
                << e.what() << std::endl;
            exit(1);
        }
        catch (std::exception &e)
        {
            // pass
        }

        try
        {
            handleSettings(config.configFile, config);
        }
        catch (std::exception &e)
        {
            std::cout
                << std::endl
                << "There was an error parsing the specified configuration file. Please check the file and try again"
                << std::endl
                << e.what() << std::endl;
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
        try
        {
            asFile(config, config.outputFile);
            std::cout << getProjectCLIHeader() << "Configuration saved to: " << config.outputFile << std::endl;
            exit(0);
        }
        catch (std::exception &e)
        {
            std::cout << getProjectCLIHeader() << "Could not save configuration to: " << config.outputFile << std::endl
                      << e.what() << std::endl;
            exit(1);
        }
    }

    /* If we were given the resync arg, we're deleting everything */
    if (config.resync)
    {
        std::error_code ec;

        std::vector<fs::path> removablePaths = {
            fs::path(config.dataDirectory) / CryptoNote::parameters::P2P_NET_DATA_FILENAME,
            fs::path(config.dataDirectory) / "DB"
        };

        for (const auto path : removablePaths)
        {
            fs::remove_all(path, ec);

            if (ec)
            {
                std::cout << "Could not delete data path: " << path << std::endl;
                exit(1);
            }
        }
    }

    if (config.p2pPort < 0 || config.p2pPort > 65535)
    {
        std::cout << "P2P Port must be between 0 and 65,535" << std::endl;
        exit(1);
    }

    if (config.p2pExternalPort < 0 || config.p2pExternalPort > 65535)
    {
        std::cout << "P2P External Port must be between 0 and 65,535" << std::endl;
        exit(1);
    }

    if (config.rpcPort < 0 || config.rpcPort > 65535)
    {
        std::cout << "RPC Port must be between 0 and 65,535" << std::endl;
        exit(1);
    }

    try
    {
        fs::path cwdPath = fs::current_path();
        auto modulePath = cwdPath / temp;
        auto cfgLogFile = fs::path(config.logFile);

        if (cfgLogFile.empty())
        {
            cfgLogFile = modulePath.replace_extension(".log");
        }
        else
        {
            if (!cfgLogFile.has_parent_path())
            {
                cfgLogFile = modulePath.parent_path() / cfgLogFile;
            }
        }

        Level cfgLogLevel = static_cast<Level>(static_cast<int>(Logging::ERROR) + config.logLevel);

        // configure logging
        logManager->configure(buildLoggerConfiguration(cfgLogLevel, cfgLogFile.string()));

        Logger::logger.setLogLevel(Logger::DEBUG);

        /* New logger, for now just passing through messages to old logger */
        Logger::logger.setLogCallback([&logger](
                const std::string prettyMessage,
                const std::string message,
                const Logger::LogLevel level,
                const std::vector<Logger::LogCategory> categories) {
            Logging::Level oldLogLevel;
            std::string logColour;

            if (level == Logger::DEBUG)
            {
                oldLogLevel = Logging::DEBUGGING;
                logColour = Logging::DEFAULT;
            }
            else if (level == Logger::INFO)
            {
                oldLogLevel = Logging::INFO;
                logColour = Logging::DEFAULT;
            }
            else if (level == Logger::WARNING)
            {
                oldLogLevel = Logging::WARNING;
                logColour = Logging::RED;
            }
            else if (level == Logger::FATAL)
            {
                oldLogLevel = Logging::FATAL;
                logColour = Logging::RED;
            }
            /* setLogCallback shouldn't get called if log level is DISABLED */
            else
            {
                throw std::runtime_error("Programmer error @ setLogCallback in Daemon.cpp");
            }

            logger(oldLogLevel, logColour) << message;
        });

        logger(INFO, BRIGHT_GREEN) << getProjectCLIHeader() << std::endl;

        logger(INFO) << "Program Working Directory: " << cwdPath;

        // create objects and link them
        CryptoNote::CurrencyBuilder currencyBuilder(logManager);
        currencyBuilder.isBlockexplorer(config.enableBlockExplorer);

        try
        {
            currencyBuilder.currency();
        }
        catch (std::exception &)
        {
            std::cout << "GENESIS_COINBASE_TX_HEX constant has an incorrect value. Please launch: "
                      << CryptoNote::CRYPTONOTE_NAME << "d --print-genesis-tx" << std::endl;
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
            config.enableDbCompression);

        bool use_checkpoints = !config.checkPoints.empty();
        CryptoNote::Checkpoints checkpoints(logManager);

        if (use_checkpoints)
        {
            logger(INFO) << "Loading Checkpoints for faster initial sync...";
            if (config.checkPoints == "default")
            {
                for (const auto &cp : CryptoNote::CHECKPOINTS)
                {
                    checkpoints.addCheckpoint(cp.index, cp.blockId);
                }
                logger(INFO) << "Loaded " << CryptoNote::CHECKPOINTS.size() << " default checkpoints";
            }
            else
            {
                bool results = checkpoints.loadCheckpointsFromFile(config.checkPoints);
                if (!results)
                {
                    throw std::runtime_error("Failed to load checkpoints");
                }
            }
        }

        NetNodeConfig netNodeConfig;
        netNodeConfig.init(
            config.p2pInterface,
            config.p2pPort,
            config.p2pExternalPort,
            config.localIp,
            config.hideMyPort,
            config.dataDirectory,
            config.peers,
            config.exclusiveNodes,
            config.priorityNodes,
            config.seedNodes,
            config.p2pResetPeerstate);

        if (!Tools::create_directories_if_necessary(dbConfig.getDataDir()))
        {
            throw std::runtime_error("Can't create directory: " + dbConfig.getDataDir());
        }

        RocksDBWrapper database(logManager);
        database.init(dbConfig);
        Tools::ScopeExit dbShutdownOnExit([&database]() { database.shutdown(); });

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

        const auto ccore = std::make_shared<CryptoNote::Core>(
            currency,
            logManager,
            std::move(checkpoints),
            dispatcher,
            std::unique_ptr<IBlockchainCacheFactory>(new DatabaseBlockchainCacheFactory(database, logger.getLogger())),
            config.transactionValidationThreads
        );

        ccore->load();

        logger(INFO) << "Core initialized OK";

        /* If we were told to rewind the blockchain to a certain height
           we will remove blocks until we're back at the height specified */
        if (config.rewindToHeight > 0)
        {
            logger(INFO) << "Rewinding blockchain to: " << config.rewindToHeight << std::endl;

            ccore->rewind(config.rewindToHeight);

            logger(INFO) << "Blockchain rewound to: " << config.rewindToHeight << std::endl;
        }

        const auto cprotocol = std::make_shared<CryptoNote::CryptoNoteProtocolHandler>(
            currency,
            dispatcher,
            *ccore,
            nullptr,
            logManager
        );

        const auto p2psrv = std::make_shared<CryptoNote::NodeServer>(
            dispatcher,
            *cprotocol,
            logManager
        );

        std::string corsDomain;

        /* TODO: enable cors should not be a vector */
        if (!config.enableCors.empty()) {
            corsDomain = config.enableCors[0];
        }

        RpcMode rpcMode = RpcMode::Default;

        if (config.enableBlockExplorerDetailed)
        {
            rpcMode = RpcMode::AllMethodsEnabled;
        }
        else if (config.enableBlockExplorer)
        {
            rpcMode = RpcMode::BlockExplorerEnabled;
        }

        RpcServer rpcServer(
            config.rpcPort,
            config.rpcInterface,
            corsDomain,
            config.feeAddress,
            config.feeAmount,
            rpcMode,
            ccore,
            p2psrv,
            cprotocol
        );

        cprotocol->set_p2p_endpoint(&(*p2psrv));
        logger(INFO) << "Initializing p2p server...";
        if (!p2psrv->init(netNodeConfig))
        {
            logger(ERROR, BRIGHT_RED) << "Failed to initialize p2p server.";
            return 1;
        }

        logger(INFO) << "P2p server initialized OK";

        // Fire up the RPC Server
        logger(INFO) << "Starting core rpc server on address " << config.rpcInterface << ":" << config.rpcPort;

        rpcServer.start();

        /* Get the RPC IP address and port we are bound to */
        auto [ip, port] = rpcServer.getConnectionInfo();

        /* If we bound the RPC to 0.0.0.0, we can't reach that with a
           standard HTTP client from anywhere. Instead, let's use the
           localhost IP address to reach ourselves */
        if (ip == "0.0.0.0")
        {
            ip = "127.0.0.1";
        }

        DaemonCommandsHandler dch(*ccore, *p2psrv, logManager, ip, port);

        if (!config.noConsole)
        {
            dch.start_handling();
        }

        Tools::SignalHandler::install([&dch] {
            dch.exit({});
            dch.stop_handling();
        });

        logger(INFO) << "Starting p2p net loop...";
        p2psrv->run();
        logger(INFO) << "p2p net loop stopped";

        dch.stop_handling();

        // stop components
        logger(INFO) << "Stopping core rpc server...";
        rpcServer.stop();

        // deinitialize components
        logger(INFO) << "Deinitializing p2p...";
        p2psrv->deinit();

        cprotocol->set_p2p_endpoint(nullptr);
        ccore->save();
    }
    catch (const std::exception &e)
    {
        logger(ERROR, BRIGHT_RED) << "Exception: " << e.what();
        return 1;
    }

    logger(INFO) << "Node stopped.";
    return 0;
}
