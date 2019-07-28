// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IObservableImpl.h"
#include "ITransfersSynchronizer.h"
#include "TransfersContainer.h"
#include "logging/LoggerRef.h"

namespace CryptoNote
{
    class TransfersSubscription : public IObservableImpl<ITransfersObserver, ITransfersSubscription>
    {
      public:
        TransfersSubscription(
            const CryptoNote::Currency &currency,
            std::shared_ptr<Logging::ILogger> logger,
            const AccountSubscription &sub);

        SynchronizationStart getSyncStart();

        void onBlockchainDetach(uint32_t height);

        void onError(const std::error_code &ec, uint32_t height);

        bool advanceHeight(uint32_t height);

        const AccountKeys &getKeys() const;

        bool addTransaction(
            const TransactionBlockInfo &blockInfo,
            const ITransactionReader &tx,
            const std::vector<TransactionOutputInformationIn> &transfers);

        void deleteUnconfirmedTransaction(const Crypto::Hash &transactionHash);

        void markTransactionConfirmed(
            const TransactionBlockInfo &block,
            const Crypto::Hash &transactionHash,
            const std::vector<uint32_t> &globalIndices);

        // ITransfersSubscription
        virtual AccountPublicAddress getAddress() override;

        virtual ITransfersContainer &getContainer() override;

      private:
        Logging::LoggerRef logger;

        TransfersContainer transfers;

        AccountSubscription subscription;

        std::string m_address;
    };

} // namespace CryptoNote
