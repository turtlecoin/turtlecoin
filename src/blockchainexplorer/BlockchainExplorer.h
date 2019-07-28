// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "BlockchainExplorerErrors.h"
#include "IBlockchainExplorer.h"
#include "INode.h"
#include "common/ObserverManager.h"
#include "logging/LoggerRef.h"
#include "serialization/BinaryInputStreamSerializer.h"
#include "serialization/BinaryOutputStreamSerializer.h"
#include "wallet/WalletAsyncContextCounter.h"

#include <atomic>
#include <mutex>
#include <unordered_set>

namespace CryptoNote
{
    enum State
    {
        NOT_INITIALIZED,
        INITIALIZED
    };

    class BlockchainExplorer : public IBlockchainExplorer, public INodeObserver
    {
      public:
        BlockchainExplorer(INode &node, std::shared_ptr<Logging::ILogger> logger);

        BlockchainExplorer(const BlockchainExplorer &) = delete;

        BlockchainExplorer(BlockchainExplorer &&) = delete;

        BlockchainExplorer &operator=(const BlockchainExplorer &) = delete;

        BlockchainExplorer &operator=(BlockchainExplorer &&) = delete;

        virtual ~BlockchainExplorer();

        virtual bool addObserver(IBlockchainObserver *observer) override;

        virtual bool removeObserver(IBlockchainObserver *observer) override;

        virtual bool getBlocks(
            const std::vector<uint32_t> &blockHeights,
            std::vector<std::vector<BlockDetails>> &blocks) override;

        virtual bool
            getBlocks(const std::vector<Crypto::Hash> &blockHashes, std::vector<BlockDetails> &blocks) override;

        virtual bool getBlocks(
            uint64_t timestampBegin,
            uint64_t timestampEnd,
            uint32_t blocksNumberLimit,
            std::vector<BlockDetails> &blocks,
            uint32_t &blocksNumberWithinTimestamps) override;

        virtual bool getBlockchainTop(BlockDetails &topBlock) override;

        virtual bool getTransactions(
            const std::vector<Crypto::Hash> &transactionHashes,
            std::vector<TransactionDetails> &transactions) override;

        virtual bool getTransactionsByPaymentId(
            const Crypto::Hash &paymentId,
            std::vector<TransactionDetails> &transactions) override;

        virtual bool getPoolState(
            const std::vector<Crypto::Hash> &knownPoolTransactionHashes,
            Crypto::Hash knownBlockchainTop,
            bool &isBlockchainActual,
            std::vector<TransactionDetails> &newTransactions,
            std::vector<Crypto::Hash> &removedTransactions) override;

        virtual bool isSynchronized() override;

        virtual void init() override;

        virtual void shutdown() override;

        virtual void poolChanged() override;

        virtual void blockchainSynchronized(uint32_t topIndex) override;

        virtual void localBlockchainUpdated(uint32_t index) override;

        typedef WalletAsyncContextCounter AsyncContextCounter;

      private:
        void poolUpdateEndHandler();

        class PoolUpdateGuard
        {
          public:
            PoolUpdateGuard();

            bool beginUpdate();

            bool endUpdate();

          private:
            enum class State
            {
                NONE,
                UPDATING,
                UPDATE_REQUIRED
            };

            std::atomic<State> m_state;
        };

        bool getBlockchainTop(BlockDetails &topBlock, bool checkInitialization);

        bool getBlocks(
            const std::vector<uint32_t> &blockHeights,
            std::vector<std::vector<BlockDetails>> &blocks,
            bool checkInitialization);

        void handleBlockchainUpdatedNotification(const std::vector<std::vector<BlockDetails>> &blocks);

        BlockDetails knownBlockchainTop;

        std::unordered_map<Crypto::Hash, TransactionDetails> knownPoolState;

        std::atomic<State> state;

        std::atomic<bool> synchronized;

        std::atomic<uint32_t> observersCounter;

        Tools::ObserverManager<IBlockchainObserver> observerManager;

        std::mutex mutex;

        INode &node;

        Logging::LoggerRef logger;

        AsyncContextCounter asyncContextCounter;

        PoolUpdateGuard poolUpdateGuard;
    };
} // namespace CryptoNote
