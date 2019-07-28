// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "INode.h"
#include "logging/LoggerRef.h"

#include <string>

namespace PaymentService
{
    class NodeFactory
    {
      public:
        static CryptoNote::INode *createNode(
            const std::string &daemonAddress,
            uint16_t daemonPort,
            uint16_t initTimeout,
            std::shared_ptr<Logging::ILogger> logger);

        static CryptoNote::INode *createNodeStub();

      private:
        NodeFactory();

        ~NodeFactory();

        CryptoNote::INode *getNode(const std::string &daemonAddress, uint16_t daemonPort);

        static NodeFactory factory;
    };

} // namespace PaymentService
