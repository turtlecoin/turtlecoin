// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>
#include <boost/program_options.hpp>

namespace PaymentService
{

    class RpcNodeConfiguration
    {
    public:
        RpcNodeConfiguration();

        static void initOptions(boost::program_options::options_description &desc);

        void init(const boost::program_options::variables_map &options);

        std::string daemonHost;
        uint16_t daemonPort;
    };

} //namespace PaymentService
