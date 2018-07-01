// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "IWalletLegacy.h"

#include "Common/IInputStream.h"
#include "crypto/chacha8.h"
#include "Serialization/BinaryInputStreamSerializer.h"
#include "Transfers/TransfersSynchronizer.h"
#include "Wallet/WalletIndices.h"

namespace CryptoNote
{

    class WalletSerializerV1
    {
    public:
        WalletSerializerV1(
                ITransfersObserver &transfersObserver,
                Crypto::PublicKey &viewPublicKey,
                Crypto::SecretKey &viewSecretKey,
                uint64_t &actualBalance,
                uint64_t &pendingBalance,
                WalletsContainer &walletsContainer,
                TransfersSyncronizer &synchronizer,
                UnlockTransactionJobs &unlockTransactions,
                WalletTransactions &transactions,
                WalletTransfers &transfers,
                UncommitedTransactions &uncommitedTransactions,
                uint32_t transactionSoftLockTime
        );

        void load(const Crypto::chacha8_key &key, Common::IInputStream &source);

        struct CryptoContext
        {
            Crypto::chacha8_key key;
            Crypto::chacha8_iv iv;

            void incIv();
        };

    private:
        static const uint32_t SERIALIZATION_VERSION;

        void loadWallet(Common::IInputStream &source, const Crypto::chacha8_key &key, uint32_t version);

        void loadWalletV1(Common::IInputStream &source, const Crypto::chacha8_key &key);

        uint32_t loadVersion(Common::IInputStream &source);

        void loadIv(Common::IInputStream &source, Crypto::chacha8_iv &iv);

        void loadKeys(Common::IInputStream &source, CryptoContext &cryptoContext);

        void loadPublicKey(Common::IInputStream &source, CryptoContext &cryptoContext);

        void loadSecretKey(Common::IInputStream &source, CryptoContext &cryptoContext);

        void checkKeys();

        void loadFlags(bool &details, bool &cache, Common::IInputStream &source, CryptoContext &cryptoContext);

        void loadWallets(Common::IInputStream &source, CryptoContext &cryptoContext);

        void subscribeWallets();

        void loadBalances(Common::IInputStream &source, CryptoContext &cryptoContext);

        void loadTransfersSynchronizer(Common::IInputStream &source, CryptoContext &cryptoContext);

        void loadObsoleteSpentOutputs(Common::IInputStream &source, CryptoContext &cryptoContext);

        void loadUnlockTransactionsJobs(Common::IInputStream &source, CryptoContext &cryptoContext);

        void loadObsoleteChange(Common::IInputStream &source, CryptoContext &cryptoContext);

        void loadUncommitedTransactions(Common::IInputStream &source, CryptoContext &cryptoContext);

        void loadTransactions(Common::IInputStream &source, CryptoContext &cryptoContext);

        void loadTransfers(Common::IInputStream &source, CryptoContext &cryptoContext, uint32_t version);

        void loadWalletV1Keys(CryptoNote::BinaryInputStreamSerializer &serializer);

        void loadWalletV1Details(CryptoNote::BinaryInputStreamSerializer &serializer);

        void addWalletV1Details(const std::vector<WalletLegacyTransaction> &txs,
                                const std::vector<WalletLegacyTransfer> &trs);

        void resetCachedBalance();

        void updateTransactionsBaseStatus();

        void updateTransfersSign();

        ITransfersObserver &m_transfersObserver;
        Crypto::PublicKey &m_viewPublicKey;
        Crypto::SecretKey &m_viewSecretKey;
        uint64_t &m_actualBalance;
        uint64_t &m_pendingBalance;
        WalletsContainer &m_walletsContainer;
        TransfersSyncronizer &m_synchronizer;
        UnlockTransactionJobs &m_unlockTransactions;
        WalletTransactions &m_transactions;
        WalletTransfers &m_transfers;
        UncommitedTransactions &m_uncommitedTransactions;
        uint32_t m_transactionSoftLockTime;
    };

} //namespace CryptoNote
