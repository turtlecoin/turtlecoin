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

#include "ConfigurationManager.h"

#include <fstream>
#include <boost/program_options.hpp>

#include "Common/CommandLine.h"
#include "Common/Util.h"
#include "version.h"

namespace PaymentService {
namespace po = boost::program_options;

ConfigurationManager::ConfigurationManager() {
}

bool ConfigurationManager::init(int argc, char** argv) {
  po::options_description cmdGeneralOptions("Common Options");
  cmdGeneralOptions.add_options()
    ("config,c", po::value<std::string>(), "configuration file")
    ("help,h", "produce this help message and exit")
    ("version,v", "Output version information");

  po::options_description confGeneralOptions;

  Configuration::initOptions(cmdGeneralOptions);
  Configuration::initOptions(confGeneralOptions);

  po::options_description remoteNodeOptions("Remote Node Options");
  RpcNodeConfiguration::initOptions(remoteNodeOptions);

  po::options_description cmdOptionsDesc;
  cmdOptionsDesc.add(cmdGeneralOptions).add(remoteNodeOptions);

  po::options_description confOptionsDesc;
  confOptionsDesc.add(confGeneralOptions).add(remoteNodeOptions);

  po::variables_map cmdOptions;
  po::store(po::parse_command_line(argc, argv, cmdOptionsDesc), cmdOptions);
  po::notify(cmdOptions);

  if (cmdOptions.count("help") > 0) {
    std::cout << cmdOptionsDesc << std::endl;
    return false;
  }

  if (cmdOptions.count("version") > 0) {
    /* TODO: Take this from a variable for easier forking */
    std::cout << "turtle-service v" << PROJECT_VERSION_LONG << std::endl;
    return false;
  }

  po::variables_map allOptions;
  if (cmdOptions.count("config")) {
    std::ifstream confStream(cmdOptions["config"].as<std::string>(), std::ifstream::in);
    if (!confStream.good()) {
      throw ConfigurationError("Cannot open configuration file");
    }
    po::store(po::parse_config_file(confStream, confOptionsDesc), allOptions);
    po::notify(allOptions);
  }

  po::store(po::parse_command_line(argc, argv, cmdOptionsDesc), allOptions);
  po::notify(allOptions);

  gateConfiguration.init(allOptions);
  remoteNodeConfig.init(allOptions);
  return true;
}

} //namespace PaymentService
