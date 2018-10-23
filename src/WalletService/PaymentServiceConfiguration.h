// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <string>
#include <stdexcept>
#include <cstdint>

#include <CryptoTypes.h>
#include "crypto/hash.h"

#include <boost/program_options.hpp>

namespace PaymentService {

class ConfigurationError : public std::runtime_error {
public:
  ConfigurationError(const char* desc) : std::runtime_error(desc) {}
};

struct Configuration {
  Configuration();

  void init(const boost::program_options::variables_map& options);
  static void initOptions(boost::program_options::options_description& desc);

  std::string bindAddress;
  uint16_t bindPort;
  Crypto::Hash rpcPassword;

  std::string containerFile;
  std::string containerPassword;
  std::string secretViewKey;
  std::string secretSpendKey;
  std::string mnemonicSeed;
  std::string logFile;
  std::string serverRoot;
  std::string corsHeader;

  bool generateNewContainer;
  bool daemonize;
  bool registerService;
  bool unregisterService;
  bool printAddresses;
  bool syncFromZero;
  bool legacySecurity;

  size_t logLevel;

  uint64_t scanHeight;
};

} //namespace PaymentService
