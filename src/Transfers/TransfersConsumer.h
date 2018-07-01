// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IBlockchainSynchronizer.h"
#include "ITransfersSynchronizer.h"
#include "TransfersSubscription.h"
#include "TypeHelpers.h"

#include "crypto/crypto.h"
#include "Logging/LoggerRef.h"

#include "IObservableImpl.h"

#include <unordered_set>

namespace CryptoNote
{

    class INode;

    class TransfersConsumer : public IObservableImpl<IBlockchainConsumerObserver, IBlockchainConsumer>
    {
    public:

        TransfersConsumer(const CryptoNote::Currency &currency, INode &node, Logging::ILogger &logger,
                          const Crypto::SecretKey &viewSecret);

        ITransfersSubscription &addSubscription(const AccountSubscription &subscription);

        // returns true if no subscribers left
        bool removeSubscription(const AccountPublicAddress &address);

        ITransfersSubscription *getSubscription(const AccountPublicAddress &acc);

        void getSubscriptions(std::vector<AccountPublicAddress> &subscriptions);

        void initTransactionPool(const std::unordered_set<Crypto::Hash> &uncommitedTransactions);

        // IBlockchainConsumer
        virtual SynchronizationStart getSyncStart() override;

        virtual void onBlockchainDetach(uint32_t height) override;

        virtual uint32_t onNewBlocks(const CompleteBlock *blocks, uint32_t startHeight, uint32_t count) override;

        virtual std::error_code onPoolUpdated(const std::vector<std::unique_ptr<ITransactionReader>> &addedTransactions,
                                              const std::vector<Crypto::Hash> &deletedTransactions) override;

        virtual const std::unordered_set<Crypto::Hash> &getKnownPoolTxIds() const override;

        virtual std::error_code addUnconfirmedTransaction(const ITransactionReader &transaction) override;

        virtual void removeUnconfirmedTransaction(const Crypto::Hash &transactionHash) override;

    private:

        template<typename F>
        void forEachSubscription(F action)
        {
            for (const auto &kv : m_subscriptions)
            {
                action(*kv.second);
            }
        }

        struct PreprocessInfo
        {
            std::unordered_map<Crypto::PublicKey, std::vector<TransactionOutputInformationIn>> outputs;
            std::vector<uint32_t> globalIdxs;
        };

        std::error_code
        preprocessOutputs(const TransactionBlockInfo &blockInfo, const ITransactionReader &tx, PreprocessInfo &info);

        std::error_code processTransaction(const TransactionBlockInfo &blockInfo, const ITransactionReader &tx);

        void processTransaction(const TransactionBlockInfo &blockInfo, const ITransactionReader &tx,
                                const PreprocessInfo &info);

        void
        processOutputs(const TransactionBlockInfo &blockInfo, TransfersSubscription &sub, const ITransactionReader &tx,
                       const std::vector<TransactionOutputInformationIn> &outputs,
                       const std::vector<uint32_t> &globalIdxs, bool &contains, bool &updated);

        std::error_code getGlobalIndices(const Crypto::Hash &transactionHash, std::vector<uint32_t> &outsGlobalIndices);

        void updateSyncStart();

        SynchronizationStart m_syncStart;
        const Crypto::SecretKey m_viewSecret;
        // map { spend public key -> subscription }
        std::unordered_map<Crypto::PublicKey, std::unique_ptr<TransfersSubscription>> m_subscriptions;
        std::unordered_set<Crypto::PublicKey> m_spendKeys;
        std::unordered_set<Crypto::Hash> m_poolTxs;

        INode &m_node;
        const CryptoNote::Currency &m_currency;
        Logging::LoggerRef m_logger;
    };

}
