// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The BBSCoin Developers
// Copyright (c) 2018, The Karbo Developers
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IBlockchainSynchronizer.h"
#include "ITransfersSynchronizer.h"
#include "TypeHelpers.h"
#include "common/ObserverManager.h"
#include "logging/LoggerRef.h"

#include <cstring>
#include <memory>
#include <unordered_map>

namespace CryptoNote
{
    class Currency;
}

namespace CryptoNote
{
    class TransfersConsumer;

    class INode;

    class TransfersSyncronizer : public ITransfersSynchronizer, public IBlockchainConsumerObserver
    {
      public:
        TransfersSyncronizer(
            const CryptoNote::Currency &currency,
            std::shared_ptr<Logging::ILogger> logger,
            IBlockchainSynchronizer &sync,
            INode &node);

        virtual ~TransfersSyncronizer();

        void initTransactionPool(const std::unordered_set<Crypto::Hash> &uncommitedTransactions);

        // ITransfersSynchronizer
        virtual ITransfersSubscription &addSubscription(const AccountSubscription &acc) override;

        virtual bool removeSubscription(const AccountPublicAddress &acc) override;

        virtual void getSubscriptions(std::vector<AccountPublicAddress> &subscriptions) override;

        virtual ITransfersSubscription *getSubscription(const AccountPublicAddress &acc) override;

        virtual std::vector<Crypto::Hash> getViewKeyKnownBlocks(const Crypto::PublicKey &publicViewKey) override;

        void subscribeConsumerNotifications(
            const Crypto::PublicKey &viewPublicKey,
            ITransfersSynchronizerObserver *observer);

        void unsubscribeConsumerNotifications(
            const Crypto::PublicKey &viewPublicKey,
            ITransfersSynchronizerObserver *observer);

        void addPublicKeysSeen(
            const AccountPublicAddress &acc,
            const Crypto::Hash &transactionHash,
            const Crypto::PublicKey &outputKey);

        // IStreamSerializable
        virtual void save(std::ostream &os) override;

        virtual void load(std::istream &in) override;

      private:
        Logging::LoggerRef m_logger;

        // map { view public key -> consumer }
        typedef std::unordered_map<Crypto::PublicKey, std::unique_ptr<TransfersConsumer>> ConsumersContainer;

        ConsumersContainer m_consumers;

        typedef Tools::ObserverManager<ITransfersSynchronizerObserver> SubscribersNotifier;

        typedef std::unordered_map<Crypto::PublicKey, std::unique_ptr<SubscribersNotifier>> SubscribersContainer;

        SubscribersContainer m_subscribers;

        // std::unordered_map<AccountAddress, std::unique_ptr<TransfersConsumer>> m_subscriptions;
        IBlockchainSynchronizer &m_sync;

        INode &m_node;

        const CryptoNote::Currency &m_currency;

        virtual void
            onBlocksAdded(IBlockchainConsumer *consumer, const std::vector<Crypto::Hash> &blockHashes) override;

        virtual void onBlockchainDetach(IBlockchainConsumer *consumer, uint32_t blockIndex) override;

        virtual void onTransactionDeleteBegin(IBlockchainConsumer *consumer, Crypto::Hash transactionHash) override;

        virtual void onTransactionDeleteEnd(IBlockchainConsumer *consumer, Crypto::Hash transactionHash) override;

        virtual void onTransactionUpdated(
            IBlockchainConsumer *consumer,
            const Crypto::Hash &transactionHash,
            const std::vector<ITransfersContainer *> &containers) override;

        bool findViewKeyForConsumer(IBlockchainConsumer *consumer, Crypto::PublicKey &viewKey) const;

        SubscribersContainer::const_iterator findSubscriberForConsumer(IBlockchainConsumer *consumer) const;
    };

} // namespace CryptoNote
