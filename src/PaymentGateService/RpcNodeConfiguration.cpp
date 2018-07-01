// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "RpcNodeConfiguration.h"

namespace PaymentService
{

    namespace po = boost::program_options;

    RpcNodeConfiguration::RpcNodeConfiguration()
    {
        daemonHost = "";
        daemonPort = 0;
    }

    void RpcNodeConfiguration::initOptions(boost::program_options::options_description &desc)
    {
        desc.add_options()
                ("daemon-address", po::value<std::string>()->default_value("localhost"), "turtlecoind address")
                ("daemon-port", po::value<uint16_t>()->default_value(11898), "daemon port");
    }

    void RpcNodeConfiguration::init(const boost::program_options::variables_map &options)
    {
        if (options.count("daemon-address") != 0 && (!options["daemon-address"].defaulted() || daemonHost.empty()))
        {
            daemonHost = options["daemon-address"].as<std::string>();
        }

        if (options.count("daemon-port") != 0 && (!options["daemon-port"].defaulted() || daemonPort == 0))
        {
            daemonPort = options["daemon-port"].as<uint16_t>();
        }
    }

} //namespace PaymentService
