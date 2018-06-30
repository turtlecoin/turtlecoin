// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "INode.h"
// #include "WalletSynchronizationContext.h"
#include "WalletLegacy/WalletSendTransactionContext.h"
#include "WalletLegacy/WalletLegacyEvent.h"

#include <boost/optional.hpp>

#include <deque>
#include <functional>
#include <memory>

namespace CryptoNote
{

    class WalletRequest
    {
    public:
        typedef std::function<void(std::deque<std::shared_ptr<WalletLegacyEvent>> &events,
                                   boost::optional<std::shared_ptr<WalletRequest> > &nextRequest,
                                   std::error_code ec)> Callback;

        virtual ~WalletRequest()
        {};

        virtual void perform(INode &node, std::function<void(WalletRequest::Callback, std::error_code)> cb) = 0;
    };

    class WalletGetRandomOutsByAmountsRequest : public WalletRequest
    {
    public:
        WalletGetRandomOutsByAmountsRequest(const std::vector<uint64_t> &amounts, uint64_t outsCount,
                                            std::shared_ptr<SendTransactionContext> context, Callback cb) :
                m_amounts(amounts), m_outsCount(outsCount), m_context(context), m_cb(cb)
        {};

        virtual ~WalletGetRandomOutsByAmountsRequest()
        {};

        virtual void perform(INode &node, std::function<void(WalletRequest::Callback, std::error_code)> cb) override
        {
            node.getRandomOutsByAmounts(std::move(m_amounts), static_cast<uint16_t>(m_outsCount),
                                        std::ref(m_context->outs), std::bind(cb, m_cb, std::placeholders::_1));
        };

    private:
        std::vector<uint64_t> m_amounts;
        uint64_t m_outsCount;
        std::shared_ptr<SendTransactionContext> m_context;
        Callback m_cb;
    };

    class WalletRelayTransactionRequest : public WalletRequest
    {
    public:
        WalletRelayTransactionRequest(const CryptoNote::Transaction &tx, Callback cb) : m_tx(tx), m_cb(cb)
        {};

        virtual ~WalletRelayTransactionRequest()
        {};

        virtual void perform(INode &node, std::function<void(WalletRequest::Callback, std::error_code)> cb) override
        {
            node.relayTransaction(m_tx, std::bind(cb, m_cb, std::placeholders::_1));
        }

    private:
        CryptoNote::Transaction m_tx;
        Callback m_cb;
    };

} //namespace CryptoNote
