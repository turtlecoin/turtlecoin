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

#include "PaymentServiceConfiguration.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <boost/program_options.hpp>

#include "Logging/ILogger.h"

namespace po = boost::program_options;

namespace PaymentService {

Configuration::Configuration() {
  generateNewContainer = false;
  daemonize = false;
  registerService = false;
  unregisterService = false;
  containerPassword = "";
  logFile = "service.log";
  printAddresses = false;
  syncFromZero = false;
  logLevel = Logging::INFO;
  bindAddress = "";
  bindPort = 0;
  secretViewKey = "";
  secretSpendKey = "";
  mnemonicSeed = "";
  rpcPassword = "";
  legacySecurity = false;
  corsHeader = "";
  scanHeight = 0;
}

void Configuration::initOptions(boost::program_options::options_description& desc) {
  desc.add_options()
      ("bind-address", po::value<std::string>()->default_value("127.0.0.1"), "payment service bind address")
      ("bind-port", po::value<uint16_t>()->default_value(8070), "payment service bind port")
      ("rpc-password", po::value<std::string>(), "Specify the password to access the rpc server.")
      ("rpc-legacy-security", "Enable legacy mode (no password for RPC). WARNING: INSECURE. USE ONLY AS A LAST RESORT.")
      ("container-file,w", po::value<std::string>(), "container file")
      ("container-password,p", po::value<std::string>(), "container password")
      ("generate-container,g", "generate new container file with one wallet and exit")
      ("view-key", po::value<std::string>(), "generate a container with this secret key view")
      ("spend-key", po::value<std::string>(), "generate a container with this secret spend key")
      ("mnemonic-seed", po::value<std::string>(), "generate a container with this mnemonic seed")
      ("daemon,d", "run as daemon in Unix or as service in Windows")
#ifdef _WIN32
      ("register-service", "register service and exit (Windows only)")
      ("unregister-service", "unregister service and exit (Windows only)")
#endif
      ("log-file,l", po::value<std::string>(), "log file")
      ("server-root", po::value<std::string>(), "server root. The service will use it as working directory. Don't set it if don't want to change it")
      ("log-level", po::value<size_t>(), "log level")
      ("SYNC_FROM_ZERO", "sync from timestamp 0")
      ("address", "print wallet addresses and exit")
      ("enable-cors", po::value<std::string>(), "Adds header 'Access-Control-Allow-Origin' to walletd's RPC responses. Uses the value as domain. Use * for all.")
      ("scan-height", po::value<uint64_t>(), "The height to begin scanning a wallet from");
}

void Configuration::init(const boost::program_options::variables_map& options) {
  if (options.count("daemon") != 0) {
    daemonize = true;
  }

  if (options.count("register-service") != 0) {
    registerService = true;
  }

  if (options.count("unregister-service") != 0) {
    unregisterService = true;
  }

  if (registerService && unregisterService) {
    throw ConfigurationError("It's impossible to use both \"register-service\" and \"unregister-service\" at the same time");
  }

  if (options.count("log-file") != 0) {
    logFile = options["log-file"].as<std::string>();
  }

  if (options.count("log-level") != 0) {
    logLevel = options["log-level"].as<size_t>();
    if (logLevel > Logging::TRACE) {
      std::string error = "log-level option must be in " + std::to_string(Logging::FATAL) +  ".." + std::to_string(Logging::TRACE) + " interval";
      throw ConfigurationError(error.c_str());
    }
  }

  if (options.count("server-root") != 0) {
    serverRoot = options["server-root"].as<std::string>();
  }

  if (options.count("bind-address") != 0 && (!options["bind-address"].defaulted() || bindAddress.empty())) {
    bindAddress = options["bind-address"].as<std::string>();
  }

  if (options.count("bind-port") != 0 && (!options["bind-port"].defaulted() || bindPort == 0)) {
    bindPort = options["bind-port"].as<uint16_t>();
  }

  if (options.count("container-file") != 0) {
    containerFile = options["container-file"].as<std::string>();
  }

  if (!std::ifstream(containerFile) && options.count("generate-container") == 0)
  {
      if (std::ifstream(containerFile + ".wallet"))
      {
        throw ConfigurationError(("The wallet file specified doesn't exist, did you mean: " + containerFile + ".wallet?").c_str());
      }
      else
      {
        throw ConfigurationError("The file specified doesn't exist; maybe check your spelling?");
      }
  }

  if (options.count("container-password") != 0) {
    containerPassword = options["container-password"].as<std::string>();
  }

  if (options.count("generate-container") != 0) {
    generateNewContainer = true;
  }

  if (options.count("view-key") != 0)
  {
    if (!generateNewContainer)
    {
      throw ConfigurationError("generate-container parameter is required");
    }
    secretViewKey = options["view-key"].as<std::string>();
  }

  if (options.count("spend-key") != 0)
  {
    if (!generateNewContainer)
    {
      throw ConfigurationError("generate-container parameter is required");
    }
    secretSpendKey = options["spend-key"].as<std::string>();
  }

  if (options.count("mnemonic-seed") != 0)
  {
    if (!generateNewContainer)
    {
      throw ConfigurationError("generate-container parameter is required");
    }
    else if (options.count("spend-key") != 0 || options.count("view-key") != 0)
    {
      throw ConfigurationError("Cannot specify import via both mnemonic seed and private keys");
    }
    mnemonicSeed = options["mnemonic-seed"].as<std::string>();
  }

  if (options.count("address") != 0) {
    printAddresses = true;
  }

  if (options.count("SYNC_FROM_ZERO") != 0) {
    syncFromZero = true;
  }
  if (!registerService && !unregisterService) {
    if (containerFile.empty()) {
      throw ConfigurationError("container-file parameter are required");
    }
  }

  // If generating a container skip the authentication parameters.
  if (generateNewContainer) {
    return;
  }

  // Check for the authentication parameters
  if ((options.count("rpc-password") == 0) && (options.count("rpc-legacy-security") == 0)) {
    throw ConfigurationError("Please specify an RPC password or use the --rpc-legacy-security flag.");
  }

  if (options.count("rpc-legacy-security") != 0) {
    legacySecurity = true;
  }
  else {
    rpcPassword = options["rpc-password"].as<std::string>();
  }

  if (options.count("enable-cors") != 0) {
    corsHeader = options["enable-cors"].as<std::string>();
  }

  if (options.count("scan-height") != 0) {
    scanHeight = options["scan-height"].as<uint64_t>();
  }

}

} //namespace PaymentService
