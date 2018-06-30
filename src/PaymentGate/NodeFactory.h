// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "INode.h"

#include <string>

#include "Logging/LoggerRef.h"

namespace PaymentService
{

    class NodeFactory
    {
    public:
        static CryptoNote::INode *
        createNode(const std::string &daemonAddress, uint16_t daemonPort, Logging::ILogger &logger);

        static CryptoNote::INode *createNodeStub();

    private:
        NodeFactory();

        ~NodeFactory();

        CryptoNote::INode *getNode(const std::string &daemonAddress, uint16_t daemonPort);

        static NodeFactory factory;
    };

} //namespace PaymentService
