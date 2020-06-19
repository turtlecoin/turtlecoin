// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The Galaxia Project Developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <WalletTypes.h>
#include <algorithm>
#include <common/CryptoNoteTools.h>
#include <common/Math.h>
#include <common/MemoryInputStream.h>
#include <common/ShuffleGenerator.h>
#include <common/TransactionExtra.h>
#include <config/Constants.h>
#include <cryptonotecore/BlockchainCache.h>
#include <cryptonotecore/BlockchainStorage.h>
#include <cryptonotecore/BlockchainUtils.h>
#include <cryptonotecore/Core.h>
#include <cryptonotecore/CoreErrors.h>
#include <cryptonotecore/CryptoNoteFormatUtils.h>
#include <cryptonotecore/ITimeProvider.h>
#include <cryptonotecore/MemoryBlockchainStorage.h>
#include <cryptonotecore/Mixins.h>
#include <cryptonotecore/TransactionApi.h>
#include <cryptonotecore/TransactionPool.h>
#include <cryptonotecore/TransactionPoolCleaner.h>
#include <cryptonotecore/UpgradeManager.h>
#include <cryptonotecore/ValidateTransaction.h>
#include <cryptonoteprotocol/CryptoNoteProtocolHandlerCommon.h>
#include <numeric>
#include <set>
#include <system/Timer.h>
#include <unordered_set>
#include <utilities/Container.h>
#include <utilities/FormatTools.h>
#include <utilities/LicenseCanary.h>
#include <utilities/ParseExtra.h>

using namespace Crypto;

namespace CryptoNote
{
    namespace
    {
        template<class T> std::vector<T> preallocateVector(size_t elements)
        {
            std::vector<T> vect;
            vect.reserve(elements);
            return vect;
        }

        UseGenesis addGenesisBlock = UseGenesis(true);

        class TransactionSpentInputsChecker
        {
          public:
            bool haveSpentInputs(const Transaction &transaction)
            {
                for (const auto &input : transaction.inputs)
                {
                    if (input.type() == typeid(KeyInput))
                    {
                        auto inserted = alreadySpentKeyImages.insert(boost::get<KeyInput>(input).keyImage);
                        if (!inserted.second)
                        {
                            return true;
                        }
                    }
                }

                return false;
            }

          private:
            std::unordered_set<Crypto::KeyImage> alreadySpentKeyImages;
        };

        inline IBlockchainCache *findIndexInChain(IBlockchainCache *blockSegment, const Crypto::Hash &blockHash)
        {
            assert(blockSegment != nullptr);
            while (blockSegment != nullptr)
            {
                if (blockSegment->hasBlock(blockHash))
                {
                    return blockSegment;
                }

                blockSegment = blockSegment->getParent();
            }

            return nullptr;
        }

        inline IBlockchainCache *findIndexInChain(IBlockchainCache *blockSegment, uint32_t blockIndex)
        {
            assert(blockSegment != nullptr);
            while (blockSegment != nullptr)
            {
                if (blockIndex >= blockSegment->getStartBlockIndex()
                    && blockIndex < blockSegment->getStartBlockIndex() + blockSegment->getBlockCount())
                {
                    return blockSegment;
                }

                blockSegment = blockSegment->getParent();
            }

            return nullptr;
        }

        size_t getMaximumTransactionAllowedSize(size_t blockSizeMedian, const Currency &currency)
        {
            assert(blockSizeMedian * 2 > currency.minerTxBlobReservedSize());

            return blockSizeMedian * 2 - currency.minerTxBlobReservedSize();
        }

        BlockTemplate extractBlockTemplate(const RawBlock &block)
        {
            BlockTemplate blockTemplate;
            if (!fromBinaryArray(blockTemplate, block.block))
            {
                throw std::system_error(make_error_code(error::AddBlockErrorCode::DESERIALIZATION_FAILED));
            }

            return blockTemplate;
        }

        Crypto::Hash getBlockHash(const RawBlock &block)
        {
            BlockTemplate blockTemplate = extractBlockTemplate(block);
            return CachedBlock(blockTemplate).getBlockHash();
        }

        TransactionValidatorState extractSpentOutputs(const CachedTransaction &transaction)
        {
            TransactionValidatorState spentOutputs;
            const auto &cryptonoteTransaction = transaction.getTransaction();

            for (const auto &input : cryptonoteTransaction.inputs)
            {
                if (input.type() == typeid(KeyInput))
                {
                    const KeyInput &in = boost::get<KeyInput>(input);
                    bool r = spentOutputs.spentKeyImages.insert(in.keyImage).second;
                    if (r)
                    {
                    }
                    assert(r);
                }
                else
                {
                    assert(false);
                }
            }

            return spentOutputs;
        }

        TransactionValidatorState extractSpentOutputs(const std::vector<CachedTransaction> &transactions)
        {
            TransactionValidatorState resultOutputs;
            for (const auto &transaction : transactions)
            {
                auto transactionOutputs = extractSpentOutputs(transaction);
                mergeStates(resultOutputs, transactionOutputs);
            }

            return resultOutputs;
        }

        int64_t getEmissionChange(
            const Currency &currency,
            IBlockchainCache &segment,
            uint32_t previousBlockIndex,
            const CachedBlock &cachedBlock,
            uint64_t cumulativeSize,
            uint64_t cumulativeFee)
        {
            uint64_t reward = 0;
            int64_t emissionChange = 0;
            auto alreadyGeneratedCoins = segment.getAlreadyGeneratedCoins(previousBlockIndex);
            auto lastBlocksSizes =
                segment.getLastBlocksSizes(currency.rewardBlocksWindow(), previousBlockIndex, addGenesisBlock);
            auto blocksSizeMedian = Common::medianValue(lastBlocksSizes);
            if (!currency.getBlockReward(
                    cachedBlock.getBlock().majorVersion,
                    blocksSizeMedian,
                    cumulativeSize,
                    alreadyGeneratedCoins,
                    cumulativeFee,
                    reward,
                    emissionChange))
            {
                throw std::system_error(make_error_code(error::BlockValidationError::CUMULATIVE_BLOCK_SIZE_TOO_BIG));
            }

            return emissionChange;
        }

        uint32_t findCommonRoot(IMainChainStorage &storage, IBlockchainCache &rootSegment)
        {
            assert(storage.getBlockCount());
            assert(rootSegment.getBlockCount());
            assert(rootSegment.getStartBlockIndex() == 0);
            assert(getBlockHash(storage.getBlockByIndex(0)) == rootSegment.getBlockHash(0));

            uint32_t left = 0;
            uint32_t right = std::min(storage.getBlockCount() - 1, rootSegment.getBlockCount() - 1);
            while (left != right)
            {
                assert(right >= left);
                uint32_t checkElement = left + (right - left) / 2 + 1;
                if (getBlockHash(storage.getBlockByIndex(checkElement)) == rootSegment.getBlockHash(checkElement))
                {
                    left = checkElement;
                }
                else
                {
                    right = checkElement - 1;
                }
            }

            return left;
        }

        const std::chrono::seconds OUTDATED_TRANSACTION_POLLING_INTERVAL = std::chrono::seconds(60);

    } // namespace

    Core::Core(
        const Currency &currency,
        std::shared_ptr<Logging::ILogger> logger,
        Checkpoints &&checkpoints,
        System::Dispatcher &dispatcher,
        std::unique_ptr<IBlockchainCacheFactory> &&blockchainCacheFactory,
        std::unique_ptr<IMainChainStorage> &&mainchainStorage,
        const uint32_t transactionValidationThreads):
        currency(currency),
        dispatcher(dispatcher),
        contextGroup(dispatcher),
        logger(logger, "Core"),
        checkpoints(std::move(checkpoints)),
        upgradeManager(new UpgradeManager()),
        blockchainCacheFactory(std::move(blockchainCacheFactory)),
        mainChainStorage(std::move(mainchainStorage)),
        initialized(false),
        m_transactionValidationThreadPool(transactionValidationThreads)
    {
        upgradeManager->addMajorBlockVersion(BLOCK_MAJOR_VERSION_2, currency.upgradeHeight(BLOCK_MAJOR_VERSION_2));
        upgradeManager->addMajorBlockVersion(BLOCK_MAJOR_VERSION_3, currency.upgradeHeight(BLOCK_MAJOR_VERSION_3));
        upgradeManager->addMajorBlockVersion(BLOCK_MAJOR_VERSION_4, currency.upgradeHeight(BLOCK_MAJOR_VERSION_4));
        upgradeManager->addMajorBlockVersion(BLOCK_MAJOR_VERSION_5, currency.upgradeHeight(BLOCK_MAJOR_VERSION_5));
        upgradeManager->addMajorBlockVersion(BLOCK_MAJOR_VERSION_6, currency.upgradeHeight(BLOCK_MAJOR_VERSION_6));
        upgradeManager->addMajorBlockVersion(BLOCK_MAJOR_VERSION_7, currency.upgradeHeight(BLOCK_MAJOR_VERSION_7));

        transactionPool = std::unique_ptr<ITransactionPoolCleanWrapper>(new TransactionPoolCleanWrapper(
            std::unique_ptr<ITransactionPool>(new TransactionPool(logger)),
            std::unique_ptr<ITimeProvider>(new RealTimeProvider()),
            logger,
            currency.mempoolTxLiveTime()));
    }

    Core::~Core()
    {
        transactionPool->flush();
        contextGroup.interrupt();
        contextGroup.wait();
    }

    bool Core::addMessageQueue(MessageQueue<BlockchainMessage> &messageQueue)
    {
        return queueList.insert(messageQueue);
    }

    bool Core::removeMessageQueue(MessageQueue<BlockchainMessage> &messageQueue)
    {
        return queueList.remove(messageQueue);
    }

    bool Core::notifyObservers(BlockchainMessage &&msg) /* noexcept */
    {
        try
        {
            for (auto &queue : queueList)
            {
                queue.push(std::move(msg));
            }
            return true;
        }
        catch (std::exception &e)
        {
            logger(Logging::WARNING) << "failed to notify observers: " << e.what();
            return false;
        }
    }

    uint32_t Core::getTopBlockIndex() const
    {
        assert(!chainsStorage.empty());
        assert(!chainsLeaves.empty());
        throwIfNotInitialized();

        return chainsLeaves[0]->getTopBlockIndex();
    }

    Crypto::Hash Core::getTopBlockHash() const
    {
        assert(!chainsStorage.empty());
        assert(!chainsLeaves.empty());

        throwIfNotInitialized();

        return chainsLeaves[0]->getTopBlockHash();
    }

    Crypto::Hash Core::getBlockHashByIndex(uint32_t blockIndex) const
    {
        assert(!chainsStorage.empty());
        assert(!chainsLeaves.empty());

        throwIfNotInitialized();

        if (blockIndex > getTopBlockIndex())
        {
            return Constants::NULL_HASH;
        }

        return chainsLeaves[0]->getBlockHash(blockIndex);
    }

    uint64_t Core::getBlockTimestampByIndex(uint32_t blockIndex) const
    {
        assert(!chainsStorage.empty());
        assert(!chainsLeaves.empty());
        assert(blockIndex <= getTopBlockIndex());

        throwIfNotInitialized();

        auto timestamps = chainsLeaves[0]->getLastTimestamps(1, blockIndex, addGenesisBlock);
        assert(timestamps.size() == 1);

        return timestamps[0];
    }

    bool Core::hasBlock(const Crypto::Hash &blockHash) const
    {
        throwIfNotInitialized();
        return findSegmentContainingBlock(blockHash) != nullptr;
    }

    BlockTemplate Core::getBlockByIndex(uint32_t index) const
    {
        assert(!chainsStorage.empty());
        assert(!chainsLeaves.empty());
        assert(index <= getTopBlockIndex());

        throwIfNotInitialized();
        IBlockchainCache *segment = findMainChainSegmentContainingBlock(index);
        assert(segment != nullptr);

        return restoreBlockTemplate(segment, index);
    }

    BlockTemplate Core::getBlockByHash(const Crypto::Hash &blockHash) const
    {
        assert(!chainsStorage.empty());
        assert(!chainsLeaves.empty());

        throwIfNotInitialized();
        IBlockchainCache *segment =
            findMainChainSegmentContainingBlock(blockHash); // TODO should it be requested from the main chain?
        if (segment == nullptr)
        {
            throw std::runtime_error("Requested hash wasn't found in main blockchain");
        }

        uint32_t blockIndex = segment->getBlockIndex(blockHash);

        return restoreBlockTemplate(segment, blockIndex);
    }

    std::vector<Crypto::Hash> Core::buildSparseChain() const
    {
        throwIfNotInitialized();
        Crypto::Hash topBlockHash = chainsLeaves[0]->getTopBlockHash();
        return doBuildSparseChain(topBlockHash);
    }

    std::vector<RawBlock> Core::getBlocks(uint32_t minIndex, uint32_t count) const
    {
        assert(!chainsStorage.empty());
        assert(!chainsLeaves.empty());

        throwIfNotInitialized();

        std::vector<RawBlock> blocks;
        if (count > 0)
        {
            auto cache = chainsLeaves[0];
            auto maxIndex = std::min(minIndex + count - 1, cache->getTopBlockIndex());
            blocks.reserve(count);
            while (cache)
            {
                if (cache->getTopBlockIndex() >= maxIndex)
                {
                    auto minChainIndex = std::max(minIndex, cache->getStartBlockIndex());
                    for (; minChainIndex <= maxIndex; --maxIndex)
                    {
                        blocks.emplace_back(cache->getBlockByIndex(maxIndex));
                        if (maxIndex == 0)
                        {
                            break;
                        }
                    }
                }

                if (blocks.size() == count)
                {
                    break;
                }

                cache = cache->getParent();
            }
        }
        std::reverse(blocks.begin(), blocks.end());

        return blocks;
    }

    void Core::getBlocks(
        const std::vector<Crypto::Hash> &blockHashes,
        std::vector<RawBlock> &blocks,
        std::vector<Crypto::Hash> &missedHashes) const
    {
        throwIfNotInitialized();

        for (const auto &hash : blockHashes)
        {
            IBlockchainCache *blockchainSegment = findSegmentContainingBlock(hash);
            if (blockchainSegment == nullptr)
            {
                missedHashes.push_back(hash);
            }
            else
            {
                uint32_t blockIndex = blockchainSegment->getBlockIndex(hash);
                assert(blockIndex <= blockchainSegment->getTopBlockIndex());

                blocks.push_back(blockchainSegment->getBlockByIndex(blockIndex));
            }
        }
    }

    void Core::copyTransactionsToPool(IBlockchainCache *alt)
    {
        assert(alt != nullptr);
        while (alt != nullptr)
        {
            if (mainChainSet.count(alt) != 0)
            {
                break;
            }
            auto transactions = alt->getRawTransactions(alt->getTransactionHashes());
            for (auto &transaction : transactions)
            {
                const auto [success, error] = addTransactionToPool(std::move(transaction));
                if (success)
                {
                    // TODO: send notification
                }
            }
            alt = alt->getParent();
        }
    }

    bool Core::queryBlocks(
        const std::vector<Crypto::Hash> &blockHashes,
        uint64_t timestamp,
        uint32_t &startIndex,
        uint32_t &currentIndex,
        uint32_t &fullOffset,
        std::vector<BlockFullInfo> &entries) const
    {
        assert(entries.empty());
        assert(!chainsLeaves.empty());
        assert(!chainsStorage.empty());
        throwIfNotInitialized();

        try
        {
            IBlockchainCache *mainChain = chainsLeaves[0];
            currentIndex = mainChain->getTopBlockIndex();

            startIndex = findBlockchainSupplement(blockHashes); // throws

            fullOffset = mainChain->getTimestampLowerBoundBlockIndex(timestamp);
            if (fullOffset < startIndex)
            {
                fullOffset = startIndex;
            }

            size_t hashesPushed =
                pushBlockHashes(startIndex, fullOffset, BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT, entries);

            if (startIndex + hashesPushed != fullOffset)
            {
                return true;
            }

            fillQueryBlockFullInfo(fullOffset, currentIndex, BLOCKS_SYNCHRONIZING_DEFAULT_COUNT, entries);

            return true;
        }
        catch (std::exception &)
        {
            // TODO log
            return false;
        }
    }

    bool Core::queryBlocksLite(
        const std::vector<Crypto::Hash> &knownBlockHashes,
        uint64_t timestamp,
        uint32_t &startIndex,
        uint32_t &currentIndex,
        uint32_t &fullOffset,
        std::vector<BlockShortInfo> &entries) const
    {
        assert(entries.empty());
        assert(!chainsLeaves.empty());
        assert(!chainsStorage.empty());

        throwIfNotInitialized();

        try
        {
            IBlockchainCache *mainChain = chainsLeaves[0];
            currentIndex = mainChain->getTopBlockIndex();

            startIndex = findBlockchainSupplement(knownBlockHashes); // throws

            // Stops bug where wallets fail to sync, because timestamps have been adjusted after syncronisation.
            // check for a query of the blocks where the block index is non-zero, but the timestamp is zero
            // indicating that the originator did not know the internal time of the block, but knew which block
            // was wanted by index.  Fullfill this by getting the time of m_blocks[startIndex].timestamp.

            if (startIndex > 0 && timestamp == 0)
            {
                if (startIndex <= mainChain->getTopBlockIndex())
                {
                    RawBlock block = mainChain->getBlockByIndex(startIndex);
                    auto blockTemplate = extractBlockTemplate(block);
                    timestamp = blockTemplate.timestamp;
                }
            }

            fullOffset = mainChain->getTimestampLowerBoundBlockIndex(timestamp);
            if (fullOffset < startIndex)
            {
                fullOffset = startIndex;
            }

            size_t hashesPushed =
                pushBlockHashes(startIndex, fullOffset, BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT, entries);

            if (startIndex + static_cast<uint32_t>(hashesPushed) != fullOffset)
            {
                return true;
            }

            fillQueryBlockShortInfo(fullOffset, currentIndex, BLOCKS_SYNCHRONIZING_DEFAULT_COUNT, entries);

            return true;
        }
        catch (std::exception &e)
        {
            logger(Logging::ERROR) << "Failed to query blocks: " << e.what();
            return false;
        }
    }

    bool Core::queryBlocksDetailed(
        const std::vector<Crypto::Hash> &knownBlockHashes,
        uint64_t timestamp,
        uint64_t &startIndex,
        uint64_t &currentIndex,
        uint64_t &fullOffset,
        std::vector<BlockDetails> &entries,
        uint32_t blockCount) const
    {
        assert(entries.empty());
        assert(!chainsLeaves.empty());
        assert(!chainsStorage.empty());

        throwIfNotInitialized();

        try
        {
            if (blockCount == 0)
            {
                blockCount = BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT;
            }
            else if (blockCount == 1)
            {
                /* If we only ever request one block at a time then any attempt to sync
       via this method will not proceed */
                blockCount = 2;
            }
            else if (blockCount > BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT)
            {
                /* If we request more than the maximum defined here, chances are we are
         going to timeout or otherwise fail whether we meant it to or not as
         this is a VERY resource heavy RPC call */
                blockCount = BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT;
            }

            IBlockchainCache *mainChain = chainsLeaves[0];
            currentIndex = mainChain->getTopBlockIndex();

            startIndex = findBlockchainSupplement(knownBlockHashes); // throws

            // Stops bug where wallets fail to sync, because timestamps have been adjusted after syncronisation.
            // check for a query of the blocks where the block index is non-zero, but the timestamp is zero
            // indicating that the originator did not know the internal time of the block, but knew which block
            // was wanted by index.  Fullfill this by getting the time of m_blocks[startIndex].timestamp.

            if (startIndex > 0 && timestamp == 0)
            {
                if (startIndex <= mainChain->getTopBlockIndex())
                {
                    RawBlock block = mainChain->getBlockByIndex(startIndex);
                    auto blockTemplate = extractBlockTemplate(block);
                    timestamp = blockTemplate.timestamp;
                }
            }

            fullOffset = mainChain->getTimestampLowerBoundBlockIndex(timestamp);
            if (fullOffset < startIndex)
            {
                fullOffset = startIndex;
            }

            size_t hashesPushed = pushBlockHashes(startIndex, fullOffset, blockCount, entries);

            if (startIndex + static_cast<uint32_t>(hashesPushed) != fullOffset)
            {
                return true;
            }

            fillQueryBlockDetails(fullOffset, currentIndex, blockCount, entries);

            return true;
        }
        catch (std::exception &e)
        {
            logger(Logging::ERROR) << "Failed to query blocks: " << e.what();
            return false;
        }
    }

    /* Transaction hashes = The hashes the wallet wants to query.
       transactions in pool - We'll add hashes to this if the transaction is in the pool
       transactions in block - We'll add hashes to this if the transaction is in a block
       transactions unknown - We'll add hashes to this if we don't know about them - possibly fell out the tx pool */
    bool Core::getTransactionsStatus(
        std::unordered_set<Crypto::Hash> transactionHashes,
        std::unordered_set<Crypto::Hash> &transactionsInPool,
        std::unordered_set<Crypto::Hash> &transactionsInBlock,
        std::unordered_set<Crypto::Hash> &transactionsUnknown) const
    {
        throwIfNotInitialized();

        try
        {
            const auto txs = transactionPool->getTransactionHashes();

            /* Pop into a set for quicker .find() */
            std::unordered_set<Crypto::Hash> poolTransactions(txs.begin(), txs.end());

            for (const auto hash : transactionHashes)
            {
                if (poolTransactions.find(hash) != poolTransactions.end())
                {
                    /* It's in the pool */
                    transactionsInPool.insert(hash);
                }
                else if (findSegmentContainingTransaction(hash) != nullptr)
                {
                    /* It's in a block */
                    transactionsInBlock.insert(hash);
                }
                else
                {
                    /* We don't know anything about it */
                    transactionsUnknown.insert(hash);
                }
            }

            return true;
        }
        catch (std::exception &e)
        {
            logger(Logging::ERROR) << "Failed to get transactions status: " << e.what();
            return false;
        }
    }

    /* Known block hashes = The hashes the wallet knows about. We'll give blocks starting from this hash.
       Timestamp = The timestamp to start giving blocks from, if knownBlockHashes is empty. Used for syncing a new
       wallet. walletBlocks = The returned vector of blocks */
    bool Core::getWalletSyncData(
        const std::vector<Crypto::Hash> &knownBlockHashes,
        const uint64_t startHeight,
        const uint64_t startTimestamp,
        const uint64_t blockCount,
        const bool skipCoinbaseTransactions,
        std::vector<WalletTypes::WalletBlockInfo> &walletBlocks,
        std::optional<WalletTypes::TopBlock> &topBlockInfo) const
    {
        throwIfNotInitialized();

        try
        {
            IBlockchainCache *mainChain = chainsLeaves[0];

            /* Current height */
            uint64_t currentIndex = mainChain->getTopBlockIndex();
            Crypto::Hash currentHash = mainChain->getTopBlockHash();

            uint64_t actualBlockCount = std::min(BLOCKS_SYNCHRONIZING_DEFAULT_COUNT, blockCount);

            if (actualBlockCount == 0)
            {
                actualBlockCount = BLOCKS_SYNCHRONIZING_DEFAULT_COUNT;
            }

            auto [success, timestampBlockHeight] = mainChain->getBlockHeightForTimestamp(startTimestamp);

            /* If no timestamp given, occasionaly the daemon returns a non zero
           block height... for some reason. Set it back to zero if we didn't
           give a timestamp to fix this. */
            if (startTimestamp == 0)
            {
                timestampBlockHeight = 0;
            }

            /* If we couldn't get the first block timestamp, then the node is
           synced less than the current height, so return no blocks till we're
           synced. */
            if (startTimestamp != 0 && !success)
            {
                topBlockInfo = WalletTypes::TopBlock({currentHash, currentIndex});
                return true;
            }

            /* If a height was given, start from there, else convert the timestamp
           to a block */
            uint64_t firstBlockHeight = startHeight == 0 ? timestampBlockHeight : startHeight;

            /* The height of the last block we know about */
            uint64_t lastKnownBlockHashHeight = static_cast<uint64_t>(findBlockchainSupplement(knownBlockHashes));

            /* Start returning either from the start height, or the height of the
           last block we know about, whichever is higher */
            uint64_t startIndex = std::max(
                /* Plus one so we return the next block - default to zero if it's zero,
           otherwise genesis block will be skipped. */
                lastKnownBlockHashHeight == 0 ? 0 : lastKnownBlockHashHeight + 1,
                firstBlockHeight);

            /* Difference between the start and end */
            uint64_t blockDifference = currentIndex - startIndex;

            /* Sync actualBlockCount or the amount of blocks between
           start and end, whichever is smaller */
            uint64_t endIndex = std::min(actualBlockCount, blockDifference + 1) + startIndex;

            logger(Logging::DEBUGGING) << "\n\n"
                                       << "\n============================================="
                                       << "\n========= GetWalletSyncData summary ========="
                                       << "\n* Known block hashes size: " << knownBlockHashes.size()
                                       << "\n* Blocks requested: " << actualBlockCount
                                       << "\n* Start height: " << startHeight
                                       << "\n* Start timestamp: " << startTimestamp
                                       << "\n* Current index: " << currentIndex
                                       << "\n* Timestamp block height: " << timestampBlockHeight
                                       << "\n* First block height: " << firstBlockHeight
                                       << "\n* Last known block hash height: " << lastKnownBlockHashHeight
                                       << "\n* Start index: " << startIndex
                                       << "\n* Block difference: " << blockDifference << "\n* End index: " << endIndex
                                       << "\n============================================="
                                       << "\n\n\n";

            /* If we're fully synced, then the start index will be greater than our
           current block. */
            if (currentIndex < startIndex)
            {
                topBlockInfo = WalletTypes::TopBlock({currentHash, currentIndex});
                return true;
            }

            std::vector<RawBlock> rawBlocks;

            if (skipCoinbaseTransactions)
            {
                rawBlocks = mainChain->getNonEmptyBlocks(startIndex, actualBlockCount);
            }
            else
            {
                rawBlocks = mainChain->getBlocksByHeight(startIndex, endIndex);
            }

            for (const auto rawBlock : rawBlocks)
            {
                BlockTemplate block;

                fromBinaryArray(block, rawBlock.block);

                WalletTypes::WalletBlockInfo walletBlock;

                CachedBlock cachedBlock(block);

                walletBlock.blockHeight = cachedBlock.getBlockIndex();
                walletBlock.blockHash = cachedBlock.getBlockHash();
                walletBlock.blockTimestamp = block.timestamp;

                if (!skipCoinbaseTransactions)
                {
                    walletBlock.coinbaseTransaction = getRawCoinbaseTransaction(block.baseTransaction);
                }

                for (const auto &transaction : rawBlock.transactions)
                {
                    walletBlock.transactions.push_back(getRawTransaction(transaction));
                }

                walletBlocks.push_back(walletBlock);
            }

            if (walletBlocks.empty())
            {
                topBlockInfo = WalletTypes::TopBlock({currentHash, currentIndex});
            }

            return true;
        }
        catch (std::exception &e)
        {
            logger(Logging::ERROR) << "Failed to get wallet sync data: " << e.what();
            return false;
        }
    }

    /* Known block hashes = The hashes the wallet knows about. We'll give blocks starting from this hash.
       Timestamp = The timestamp to start giving blocks from, if knownBlockHashes is empty. Used for syncing a new
       wallet. walletBlocks = The returned vector of blocks */
    bool Core::getRawBlocks(
        const std::vector<Crypto::Hash> &knownBlockHashes,
        const uint64_t startHeight,
        const uint64_t startTimestamp,
        const uint64_t blockCount,
        const bool skipCoinbaseTransactions,
        std::vector<RawBlock> &blocks,
        std::optional<WalletTypes::TopBlock> &topBlockInfo) const
    {
        throwIfNotInitialized();

        try
        {
            IBlockchainCache *mainChain = chainsLeaves[0];

            /* Current height */
            uint64_t currentIndex = mainChain->getTopBlockIndex();
            Crypto::Hash currentHash = mainChain->getTopBlockHash();

            uint64_t actualBlockCount = std::min(BLOCKS_SYNCHRONIZING_DEFAULT_COUNT, blockCount);

            if (actualBlockCount == 0)
            {
                actualBlockCount = BLOCKS_SYNCHRONIZING_DEFAULT_COUNT;
            }

            auto [success, timestampBlockHeight] = mainChain->getBlockHeightForTimestamp(startTimestamp);

            /* If no timestamp given, occasionaly the daemon returns a non zero
           block height... for some reason. Set it back to zero if we didn't
           give a timestamp to fix this. */
            if (startTimestamp == 0)
            {
                timestampBlockHeight = 0;
            }

            /* If we couldn't get the first block timestamp, then the node is
           synced less than the current height, so return no blocks till we're
           synced. */
            if (startTimestamp != 0 && !success)
            {
                topBlockInfo = WalletTypes::TopBlock({currentHash, currentIndex});
                return true;
            }

            /* If a height was given, start from there, else convert the timestamp
           to a block */
            uint64_t firstBlockHeight = startHeight == 0 ? timestampBlockHeight : startHeight;

            /* The height of the last block we know about */
            uint64_t lastKnownBlockHashHeight = static_cast<uint64_t>(findBlockchainSupplement(knownBlockHashes));

            /* Start returning either from the start height, or the height of the
           last block we know about, whichever is higher */
            uint64_t startIndex = std::max(
                /* Plus one so we return the next block - default to zero if it's zero,
           otherwise genesis block will be skipped. */
                lastKnownBlockHashHeight == 0 ? 0 : lastKnownBlockHashHeight + 1,
                firstBlockHeight);

            /* Difference between the start and end */
            uint64_t blockDifference = currentIndex - startIndex;

            /* Sync actualBlockCount or the amount of blocks between
           start and end, whichever is smaller */
            uint64_t endIndex = std::min(actualBlockCount, blockDifference + 1) + startIndex;

            logger(Logging::DEBUGGING) << "\n\n"
                                       << "\n============================================="
                                       << "\n========= GetRawBlocks summary ========="
                                       << "\n* Known block hashes size: " << knownBlockHashes.size()
                                       << "\n* Blocks requested: " << actualBlockCount
                                       << "\n* Start height: " << startHeight
                                       << "\n* Start timestamp: " << startTimestamp
                                       << "\n* Current index: " << currentIndex
                                       << "\n* Timestamp block height: " << timestampBlockHeight
                                       << "\n* First block height: " << firstBlockHeight
                                       << "\n* Last known block hash height: " << lastKnownBlockHashHeight
                                       << "\n* Start index: " << startIndex
                                       << "\n* Block difference: " << blockDifference << "\n* End index: " << endIndex
                                       << "\n============================================="
                                       << "\n\n\n";

            /* If we're fully synced, then the start index will be greater than our
           current block. */
            if (currentIndex < startIndex)
            {
                topBlockInfo = WalletTypes::TopBlock({currentHash, currentIndex});
                return true;
            }

            if (skipCoinbaseTransactions)
            {
                blocks = mainChain->getNonEmptyBlocks(startIndex, actualBlockCount);
            }
            else
            {
                blocks = mainChain->getBlocksByHeight(startIndex, endIndex);
            }

            if (blocks.empty())
            {
                topBlockInfo = WalletTypes::TopBlock({currentHash, currentIndex});
            }

            return true;
        }
        catch (std::exception &e)
        {
            logger(Logging::ERROR) << "Failed to get wallet sync data: " << e.what();
            return false;
        }
    }

    WalletTypes::RawCoinbaseTransaction Core::getRawCoinbaseTransaction(const CryptoNote::Transaction &t)
    {
        WalletTypes::RawCoinbaseTransaction transaction;

        transaction.hash = getBinaryArrayHash(toBinaryArray(t));

        transaction.transactionPublicKey = Utilities::getTransactionPublicKeyFromExtra(t.extra);

        transaction.unlockTime = t.unlockTime;

        /* Fill in the simplified key outputs */
        for (const auto &output : t.outputs)
        {
            WalletTypes::KeyOutput keyOutput;

            keyOutput.amount = output.amount;
            keyOutput.key = boost::get<CryptoNote::KeyOutput>(output.target).key;

            transaction.keyOutputs.push_back(keyOutput);
        }

        return transaction;
    }

    WalletTypes::RawTransaction Core::getRawTransaction(const std::vector<uint8_t> &rawTX)
    {
        Transaction t;

        /* Convert the binary array to a transaction */
        fromBinaryArray(t, rawTX);

        WalletTypes::RawTransaction transaction;

        /* Get the transaction hash from the binary array */
        transaction.hash = getBinaryArrayHash(rawTX);

        Utilities::ParsedExtra parsedExtra = Utilities::parseExtra(t.extra);

        /* Transaction public key, used for decrypting transactions along with
       private view key */
        transaction.transactionPublicKey = parsedExtra.transactionPublicKey;

        /* Get the payment ID if it exists (Empty string if it doesn't) */
        transaction.paymentID = parsedExtra.paymentID;

        transaction.unlockTime = t.unlockTime;

        /* Simplify the outputs */
        for (const auto &output : t.outputs)
        {
            WalletTypes::KeyOutput keyOutput;

            keyOutput.amount = output.amount;
            keyOutput.key = boost::get<CryptoNote::KeyOutput>(output.target).key;

            transaction.keyOutputs.push_back(keyOutput);
        }

        /* Simplify the inputs */
        for (const auto &input : t.inputs)
        {
            transaction.keyInputs.push_back(boost::get<CryptoNote::KeyInput>(input));
        }

        return transaction;
    }

    std::optional<BinaryArray> Core::getTransaction(const Crypto::Hash &hash) const
    {
        throwIfNotInitialized();
        auto segment = findSegmentContainingTransaction(hash);
        if (segment != nullptr)
        {
            return segment->getRawTransactions({hash})[0];
        }
        else if (transactionPool->checkIfTransactionPresent(hash))
        {
            return transactionPool->getTransaction(hash).getTransactionBinaryArray();
        }
        else
        {
            return std::nullopt;
        }
    }

    void Core::getTransactions(
        const std::vector<Crypto::Hash> &transactionHashes,
        std::vector<BinaryArray> &transactions,
        std::vector<Crypto::Hash> &missedHashes) const
    {
        assert(!chainsLeaves.empty());
        assert(!chainsStorage.empty());
        throwIfNotInitialized();

        IBlockchainCache *segment = chainsLeaves[0];
        assert(segment != nullptr);

        std::vector<Crypto::Hash> leftTransactions = transactionHashes;

        // find in main chain
        do
        {
            std::vector<Crypto::Hash> missedTransactions;
            segment->getRawTransactions(leftTransactions, transactions, missedTransactions);

            leftTransactions = std::move(missedTransactions);
            segment = segment->getParent();
        } while (segment != nullptr && !leftTransactions.empty());

        if (leftTransactions.empty())
        {
            return;
        }

        // find in alternative chains
        for (size_t chain = 1; chain < chainsLeaves.size(); ++chain)
        {
            segment = chainsLeaves[chain];

            while (mainChainSet.count(segment) == 0 && !leftTransactions.empty())
            {
                std::vector<Crypto::Hash> missedTransactions;
                segment->getRawTransactions(leftTransactions, transactions, missedTransactions);

                leftTransactions = std::move(missedTransactions);
                segment = segment->getParent();
            }
        }

        missedHashes.insert(missedHashes.end(), leftTransactions.begin(), leftTransactions.end());
    }

    uint64_t Core::getBlockDifficulty(uint32_t blockIndex) const
    {
        throwIfNotInitialized();
        IBlockchainCache *mainChain = chainsLeaves[0];
        auto difficulties = mainChain->getLastCumulativeDifficulties(2, blockIndex, addGenesisBlock);
        if (difficulties.size() == 2)
        {
            return difficulties[1] - difficulties[0];
        }

        assert(difficulties.size() == 1);
        return difficulties[0];
    }

    // TODO: just use mainChain->getDifficultyForNextBlock() ?
    uint64_t Core::getDifficultyForNextBlock() const
    {
        throwIfNotInitialized();
        IBlockchainCache *mainChain = chainsLeaves[0];

        uint32_t topBlockIndex = mainChain->getTopBlockIndex();

        uint8_t nextBlockMajorVersion = getBlockMajorVersionForHeight(topBlockIndex);

        size_t blocksCount = std::min(
            static_cast<size_t>(topBlockIndex),
            currency.difficultyBlocksCountByBlockVersion(nextBlockMajorVersion, topBlockIndex));

        auto timestamps = mainChain->getLastTimestamps(blocksCount);
        auto difficulties = mainChain->getLastCumulativeDifficulties(blocksCount);

        return currency.getNextDifficulty(nextBlockMajorVersion, topBlockIndex, timestamps, difficulties);
    }

    std::vector<Crypto::Hash> Core::findBlockchainSupplement(
        const std::vector<Crypto::Hash> &remoteBlockIds,
        size_t maxCount,
        uint32_t &totalBlockCount,
        uint32_t &startBlockIndex) const
    {
        assert(!remoteBlockIds.empty());
        assert(remoteBlockIds.back() == getBlockHashByIndex(0));
        throwIfNotInitialized();

        totalBlockCount = getTopBlockIndex() + 1;
        startBlockIndex = findBlockchainSupplement(remoteBlockIds);

        return getBlockHashes(startBlockIndex, static_cast<uint32_t>(maxCount));
    }

    std::error_code Core::addBlock(const CachedBlock &cachedBlock, RawBlock &&rawBlock)
    {
        throwIfNotInitialized();
        uint32_t blockIndex = cachedBlock.getBlockIndex();
        Crypto::Hash blockHash = cachedBlock.getBlockHash();
        std::ostringstream os;
        os << blockIndex << " (" << blockHash << ")";
        std::string blockStr = os.str();

        logger(Logging::DEBUGGING) << "Request to add block " << blockStr;
        if (hasBlock(cachedBlock.getBlockHash()))
        {
            logger(Logging::DEBUGGING) << "Block " << blockStr << " already exists";
            return error::AddBlockErrorCode::ALREADY_EXISTS;
        }

        const auto &blockTemplate = cachedBlock.getBlock();
        const auto &previousBlockHash = blockTemplate.previousBlockHash;

        assert(rawBlock.transactions.size() == blockTemplate.transactionHashes.size());

        auto cache = findSegmentContainingBlock(previousBlockHash);
        if (cache == nullptr)
        {
            logger(Logging::DEBUGGING) << "Block " << blockStr << " rejected as orphaned";
            return error::AddBlockErrorCode::REJECTED_AS_ORPHANED;
        }

        std::vector<CachedTransaction> transactions;
        uint64_t cumulativeSize = 0;
        if (!extractTransactions(rawBlock.transactions, transactions, cumulativeSize))
        {
            logger(Logging::DEBUGGING) << "Couldn't deserialize raw block transactions in block " << blockStr;
            return error::AddBlockErrorCode::DESERIALIZATION_FAILED;
        }

        auto coinbaseTransactionSize = getObjectBinarySize(blockTemplate.baseTransaction);
        assert(coinbaseTransactionSize < std::numeric_limits<decltype(coinbaseTransactionSize)>::max());
        auto cumulativeBlockSize = coinbaseTransactionSize + cumulativeSize;
        TransactionValidatorState validatorState;

        auto previousBlockIndex = cache->getBlockIndex(previousBlockHash);

        bool addOnTop = cache->getTopBlockIndex() == previousBlockIndex;
        auto maxBlockCumulativeSize = currency.maxBlockCumulativeSize(previousBlockIndex + 1);
        if (cumulativeBlockSize > maxBlockCumulativeSize)
        {
            logger(Logging::DEBUGGING) << "Block " << blockStr << " has too big cumulative size";
            return error::BlockValidationError::CUMULATIVE_BLOCK_SIZE_TOO_BIG;
        }

        uint64_t minerReward = 0;
        auto blockValidationResult = validateBlock(cachedBlock, cache, minerReward);
        if (blockValidationResult)
        {
            logger(Logging::DEBUGGING) << "Failed to validate block " << blockStr << ": "
                                       << blockValidationResult.message();
            return blockValidationResult;
        }

        auto currentDifficulty = cache->getDifficultyForNextBlock(previousBlockIndex);
        if (currentDifficulty == 0)
        {
            logger(Logging::DEBUGGING) << "Block " << blockStr << " has difficulty overhead";
            return error::BlockValidationError::DIFFICULTY_OVERHEAD;
        }

        // Copyright (c) 2018-2019, The Galaxia Project Developers
        // See https://github.com/turtlecoin/turtlecoin/issues/748 for more information
        if (blockIndex >= CryptoNote::parameters::BLOCK_BLOB_SHUFFLE_CHECK_HEIGHT)
        {
            /* Check to verify that the blocktemplate suppied contains no duplicate transaction hashes */
            if (!Utilities::is_unique(blockTemplate.transactionHashes.begin(), blockTemplate.transactionHashes.end()))
            {
                return error::BlockValidationError::TRANSACTION_DUPLICATES;
            }

            /* Build a vector of the rawBlock transaction Hashes */
            std::vector<Crypto::Hash> transactionHashes {transactions.size()};

            std::transform(
                transactions.begin(), transactions.end(), transactionHashes.begin(), [](const auto &transaction) {
                    return transaction.getTransactionHash();
                });

            /* Make sure that the rawBlock transaction hashes contain no duplicates */
            if (!Utilities::is_unique(transactionHashes.begin(), transactionHashes.end()))
            {
                return error::BlockValidationError::TRANSACTION_DUPLICATES;
            }

            /* Loop through the rawBlock transaction hashes and verify that they are
               all in the blocktemplate transaction hashes */
            for (const auto &transaction : transactionHashes)
            {
                const auto search = std::find(
                    blockTemplate.transactionHashes.begin(), blockTemplate.transactionHashes.end(), transaction);

                if (search == blockTemplate.transactionHashes.end())
                {
                    return error::BlockValidationError::TRANSACTION_INCONSISTENCY;
                }
            }

            /* Ensure that the blocktemplate hashes vector matches the rawBlock transactionHashes vector */
            if (blockTemplate.transactionHashes != transactionHashes)
            {
                return error::BlockValidationError::TRANSACTION_INCONSISTENCY;
            }
        }

        uint64_t cumulativeFee = 0;

        for (const auto &transaction : transactions)
        {
            uint64_t fee = 0;
            auto transactionValidationResult =
                validateTransaction(transaction, validatorState, cache, m_transactionValidationThreadPool, fee, previousBlockIndex, false);

            if (transactionValidationResult)
            {
                const auto hash = transaction.getTransactionHash();

                logger(Logging::DEBUGGING) << "Failed to validate transaction " << hash
                                           << ": " << transactionValidationResult.message();

                if (transactionPool->checkIfTransactionPresent(hash))
                {
                    logger(Logging::DEBUGGING) << "Invalid transaction " << hash << " is present in the pool, removing";
                    transactionPool->removeTransaction(hash);
                    notifyObservers(makeDelTransactionMessage({hash}, Messages::DeleteTransaction::Reason::NotActual));
                }

                return transactionValidationResult;
            }

            cumulativeFee += fee;
        }

        uint64_t reward = 0;
        int64_t emissionChange = 0;
        auto alreadyGeneratedCoins = cache->getAlreadyGeneratedCoins(previousBlockIndex);
        auto lastBlocksSizes =
            cache->getLastBlocksSizes(currency.rewardBlocksWindow(), previousBlockIndex, addGenesisBlock);
        auto blocksSizeMedian = Common::medianValue(lastBlocksSizes);

        if (!currency.getBlockReward(
                cachedBlock.getBlock().majorVersion,
                blocksSizeMedian,
                cumulativeBlockSize,
                alreadyGeneratedCoins,
                cumulativeFee,
                reward,
                emissionChange))
        {
            logger(Logging::DEBUGGING) << "Block " << blockStr << " has too big cumulative size";
            return error::BlockValidationError::CUMULATIVE_BLOCK_SIZE_TOO_BIG;
        }

        if (minerReward != reward)
        {
            logger(Logging::DEBUGGING) << "Block reward mismatch for block " << blockStr
                                       << ". Expected reward: " << reward << ", got reward: " << minerReward;
            return error::BlockValidationError::BLOCK_REWARD_MISMATCH;
        }

        if (checkpoints.isInCheckpointZone(cachedBlock.getBlockIndex()))
        {
            if (!checkpoints.checkBlock(cachedBlock.getBlockIndex(), cachedBlock.getBlockHash()))
            {
                logger(Logging::WARNING) << "Checkpoint block hash mismatch for block " << blockStr;
                return error::BlockValidationError::CHECKPOINT_BLOCK_HASH_MISMATCH;
            }
        }
        else if (!currency.checkProofOfWork(cachedBlock, currentDifficulty))
        {
            logger(Logging::DEBUGGING) << "Proof of work too weak for block " << blockStr;
            return error::BlockValidationError::PROOF_OF_WORK_TOO_WEAK;
        }

        auto ret = error::AddBlockErrorCode::ADDED_TO_ALTERNATIVE;

        if (addOnTop)
        {
            if (cache->getChildCount() == 0)
            {
                // add block on top of leaf segment.
                auto hashes = preallocateVector<Crypto::Hash>(transactions.size());

                // TODO: exception safety
                if (cache == chainsLeaves[0])
                {
                    mainChainStorage->pushBlock(rawBlock);

                    cache->pushBlock(
                        cachedBlock,
                        transactions,
                        validatorState,
                        cumulativeBlockSize,
                        emissionChange,
                        currentDifficulty,
                        std::move(rawBlock));

                    updateBlockMedianSize();

                    /* Take the current block spent key images and run them
                       against the pool to remove any transactions that may
                       be in the pool that would now be considered invalid */
                    checkAndRemoveInvalidPoolTransactions(validatorState);

                    ret = error::AddBlockErrorCode::ADDED_TO_MAIN;
                    logger(Logging::DEBUGGING) << "Block " << blockStr << " added to main chain.";
                    if ((previousBlockIndex + 1) % 100 == 0)
                    {
                        logger(Logging::INFO) << "Block " << blockStr << " added to main chain";
                    }

                    notifyObservers(
                        makeDelTransactionMessage(std::move(hashes), Messages::DeleteTransaction::Reason::InBlock));
                }
                else
                {
                    cache->pushBlock(
                        cachedBlock,
                        transactions,
                        validatorState,
                        cumulativeBlockSize,
                        emissionChange,
                        currentDifficulty,
                        std::move(rawBlock));
                    logger(Logging::DEBUGGING) << "Block " << blockStr << " added to alternative chain.";

                    auto mainChainCache = chainsLeaves[0];
                    if (cache->getCurrentCumulativeDifficulty() > mainChainCache->getCurrentCumulativeDifficulty())
                    {
                        size_t endpointIndex = std::distance(
                            chainsLeaves.begin(), std::find(chainsLeaves.begin(), chainsLeaves.end(), cache));
                        assert(endpointIndex != chainsStorage.size());
                        assert(endpointIndex != 0);
                        std::swap(chainsLeaves[0], chainsLeaves[endpointIndex]);
                        updateMainChainSet();

                        updateBlockMedianSize();

                        /* Take the current block spent key images and run them
                           against the pool to remove any transactions that may
                           be in the pool that would now be considered invalid */
                        checkAndRemoveInvalidPoolTransactions(validatorState);

                        copyTransactionsToPool(chainsLeaves[endpointIndex]);

                        switchMainChainStorage(chainsLeaves[0]->getStartBlockIndex(), *chainsLeaves[0]);

                        ret = error::AddBlockErrorCode::ADDED_TO_ALTERNATIVE_AND_SWITCHED;

                        logger(Logging::INFO) << "Resolved: " << blockStr
                                              << ", Previous: " << chainsLeaves[endpointIndex]->getTopBlockIndex()
                                              << " (" << chainsLeaves[endpointIndex]->getTopBlockHash() << ")";
                    }
                }
            }
            else
            {
                // add block on top of segment which is not leaf! the case when we got more than one alternative block
                // on the same height
                auto newCache = blockchainCacheFactory->createBlockchainCache(currency, cache, previousBlockIndex + 1);
                cache->addChild(newCache.get());

                auto newlyForkedChainPtr = newCache.get();
                chainsStorage.emplace_back(std::move(newCache));
                chainsLeaves.push_back(newlyForkedChainPtr);

                logger(Logging::DEBUGGING) << "Resolving: " << blockStr;

                newlyForkedChainPtr->pushBlock(
                    cachedBlock,
                    transactions,
                    validatorState,
                    cumulativeBlockSize,
                    emissionChange,
                    currentDifficulty,
                    std::move(rawBlock));

                updateMainChainSet();
                updateBlockMedianSize();
            }
        }
        else
        {
            logger(Logging::DEBUGGING) << "Resolving: " << blockStr;

            auto upperSegment = cache->split(previousBlockIndex + 1);
            //[cache] is lower segment now

            assert(upperSegment->getBlockCount() > 0);
            assert(cache->getBlockCount() > 0);

            if (upperSegment->getChildCount() == 0)
            {
                // newly created segment is leaf node
                //[cache] used to be a leaf node. we have to replace it with upperSegment
                auto found = std::find(chainsLeaves.begin(), chainsLeaves.end(), cache);
                assert(found != chainsLeaves.end());

                *found = upperSegment.get();
            }

            chainsStorage.emplace_back(std::move(upperSegment));

            auto newCache = blockchainCacheFactory->createBlockchainCache(currency, cache, previousBlockIndex + 1);
            cache->addChild(newCache.get());

            auto newlyForkedChainPtr = newCache.get();
            chainsStorage.emplace_back(std::move(newCache));
            chainsLeaves.push_back(newlyForkedChainPtr);

            newlyForkedChainPtr->pushBlock(
                cachedBlock,
                transactions,
                validatorState,
                cumulativeBlockSize,
                emissionChange,
                currentDifficulty,
                std::move(rawBlock));

            updateMainChainSet();
        }

        logger(Logging::DEBUGGING) << "Block: " << blockStr << " successfully added";
        notifyOnSuccess(ret, previousBlockIndex, cachedBlock, *cache);

        return ret;
    }

    /* This method is a light version of transaction validation that is used
       to clear the transaction pool of transactions that have been invalidated
       by the addition of a block to the blockchain. As the transactions are already
       in the pool, there are only a subset of normal transaction validation
       tests that need to be completed to determine if the transaction can
       stay in the pool at this time. */
    void Core::checkAndRemoveInvalidPoolTransactions(
        const TransactionValidatorState blockTransactionsState)
    {
        auto &pool = *transactionPool;

        const auto poolHashes = pool.getTransactionHashes();

        const auto maxTransactionSize = getMaximumTransactionAllowedSize(blockMedianSize, currency);

        for (const auto poolTxHash : poolHashes)
        {
            const auto poolTx = pool.tryGetTransaction(poolTxHash);

            /* Tx got removed by another thread */
            if (!poolTx)
            {
                continue;
            }

            const auto poolTxState = extractSpentOutputs(*poolTx);

            auto [mixinSuccess, err] = Mixins::validate({*poolTx}, getTopBlockIndex());

            bool isValid = true;

            /* If the transaction is in the chain but somehow was not previously removed, fail */
            if (isTransactionInChain(poolTxHash))
            {
                isValid = false;
            }
            /* If the transaction does not have the right number of mixins, fail */
            else if (!mixinSuccess)
            {
                isValid = false;
            }
            /* If the transaction exceeds the maximum size of a transaction, fail */
            else if (poolTx->getTransactionBinaryArray().size() > maxTransactionSize)
            {
                isValid = false;
            }
            /* If the the transaction contains outputs that were spent in the new block, fail */
            else if (hasIntersections(blockTransactionsState, poolTxState))
            {
                isValid = false;
            }

            /* If the transaction is no longer valid, remove it from the pool
               and tell everyone else that they should also remove it from the pool */
            if (!isValid)
            {
                pool.removeTransaction(poolTxHash);
                notifyObservers(makeDelTransactionMessage({poolTxHash}, Messages::DeleteTransaction::Reason::NotActual));
            }
        }
    }

    /* This quickly finds out if a transaction is in the blockchain somewhere */
    bool Core::isTransactionInChain(const Crypto::Hash &txnHash)
    {
        throwIfNotInitialized();

        auto segment = findSegmentContainingTransaction(txnHash);

        if (segment != nullptr)
        {
            return true;
        }

        return false;
    }

    void Core::switchMainChainStorage(uint32_t splitBlockIndex, IBlockchainCache &newChain)
    {
        assert(mainChainStorage->getBlockCount() > splitBlockIndex);

        auto blocksToPop = mainChainStorage->getBlockCount() - splitBlockIndex;
        for (size_t i = 0; i < blocksToPop; ++i)
        {
            mainChainStorage->popBlock();
        }

        for (uint32_t index = splitBlockIndex; index <= newChain.getTopBlockIndex(); ++index)
        {
            mainChainStorage->pushBlock(newChain.getBlockByIndex(index));
        }
    }

    void Core::notifyOnSuccess(
        error::AddBlockErrorCode opResult,
        uint32_t previousBlockIndex,
        const CachedBlock &cachedBlock,
        const IBlockchainCache &cache)
    {
        switch (opResult)
        {
            case error::AddBlockErrorCode::ADDED_TO_MAIN:
                notifyObservers(makeNewBlockMessage(previousBlockIndex + 1, cachedBlock.getBlockHash()));
                break;
            case error::AddBlockErrorCode::ADDED_TO_ALTERNATIVE:
                notifyObservers(makeNewAlternativeBlockMessage(previousBlockIndex + 1, cachedBlock.getBlockHash()));
                break;
            case error::AddBlockErrorCode::ADDED_TO_ALTERNATIVE_AND_SWITCHED:
            {
                auto parent = cache.getParent();
                auto hashes = cache.getBlockHashes(cache.getStartBlockIndex(), cache.getBlockCount());
                hashes.insert(hashes.begin(), parent->getTopBlockHash());
                notifyObservers(makeChainSwitchMessage(parent->getTopBlockIndex(), std::move(hashes)));
                break;
            }
            default:
                assert(false);
                break;
        }
    }

    std::error_code Core::addBlock(RawBlock &&rawBlock)
    {
        throwIfNotInitialized();

        BlockTemplate blockTemplate;
        bool result = fromBinaryArray(blockTemplate, rawBlock.block);
        if (!result)
        {
            return error::AddBlockErrorCode::DESERIALIZATION_FAILED;
        }

        CachedBlock cachedBlock(blockTemplate);
        return addBlock(cachedBlock, std::move(rawBlock));
    }

    std::error_code Core::submitBlock(const BinaryArray &rawBlockTemplate)
    {
        throwIfNotInitialized();

        BlockTemplate blockTemplate;
        bool result = fromBinaryArray(blockTemplate, rawBlockTemplate);
        if (!result)
        {
            logger(Logging::WARNING) << "Couldn't deserialize block template";
            return error::AddBlockErrorCode::DESERIALIZATION_FAILED;
        }

        RawBlock rawBlock;
        rawBlock.block = std::move(rawBlockTemplate);

        rawBlock.transactions.reserve(blockTemplate.transactionHashes.size());

        std::scoped_lock lock(m_submitBlockMutex);

        for (const auto &transactionHash : blockTemplate.transactionHashes)
        {
            if (!transactionPool->checkIfTransactionPresent(transactionHash))
            {
                logger(Logging::WARNING) << "The transaction " << Common::podToHex(transactionHash)
                                         << " is absent in transaction pool";
                return error::BlockValidationError::TRANSACTION_ABSENT_IN_POOL;
            }

            rawBlock.transactions.emplace_back(
                transactionPool->getTransaction(transactionHash).getTransactionBinaryArray());
        }

        CachedBlock cachedBlock(blockTemplate);
        return addBlock(cachedBlock, std::move(rawBlock));
    }

    bool Core::getTransactionGlobalIndexes(const Crypto::Hash &transactionHash, std::vector<uint32_t> &globalIndexes)
        const
    {
        throwIfNotInitialized();
        IBlockchainCache *segment = chainsLeaves[0];

        bool found = false;
        while (segment != nullptr && found == false)
        {
            found = segment->getTransactionGlobalIndexes(transactionHash, globalIndexes);
            segment = segment->getParent();
        }

        if (found)
        {
            return true;
        }

        for (size_t i = 1; i < chainsLeaves.size() && found == false; ++i)
        {
            segment = chainsLeaves[i];
            while (found == false && mainChainSet.count(segment) == 0)
            {
                found = segment->getTransactionGlobalIndexes(transactionHash, globalIndexes);
                segment = segment->getParent();
            }
        }

        return found;
    }

    std::tuple<bool, std::string> Core::getRandomOutputs(
        uint64_t amount,
        uint16_t count,
        std::vector<uint32_t> &globalIndexes,
        std::vector<Crypto::PublicKey> &publicKeys) const
    {
        throwIfNotInitialized();

        if (count == 0)
        {
            return {true, ""};
        }

        auto upperBlockLimit = getTopBlockIndex() - currency.minedMoneyUnlockWindow();

        if (upperBlockLimit < currency.minedMoneyUnlockWindow())
        {
            std::string error = "Blockchain height is less than mined unlock window";
            logger(Logging::DEBUGGING) << error;

            return {false, error};
        }

        globalIndexes = chainsLeaves[0]->getRandomOutsByAmount(amount, count, getTopBlockIndex());

        if (globalIndexes.empty())
        {
            std::stringstream stream;

            stream << "Failed to get any matching outputs for amount " << amount << " ("
                   << Utilities::formatAmount(amount) << "). Further explanation here: "
                   << "https://gist.github.com/zpalmtree/80b3e80463225bcfb8f8432043cb594c\n"
                   << "Note: If you are a public node operator, you can safely ignore this message. "
                   << "It is only relevant to the user sending the transaction.";

            std::string error = stream.str();

            logger(Logging::ERROR) << error;

            return {false, error};
        }

        std::sort(globalIndexes.begin(), globalIndexes.end());

        switch (chainsLeaves[0]->extractKeyOutputKeys(
            amount, getTopBlockIndex(), {globalIndexes.data(), globalIndexes.size()}, publicKeys))
        {
            case ExtractOutputKeysResult::SUCCESS:
            {
                return {true, ""};
            }
            case ExtractOutputKeysResult::INVALID_GLOBAL_INDEX:
            {
                std::string error = "Invalid global index is given";

                logger(Logging::DEBUGGING) << error;

                return {false, error};
            }
            case ExtractOutputKeysResult::OUTPUT_LOCKED:
            {
                std::string error = "Output is locked";

                logger(Logging::DEBUGGING) << error;

                return {false, error};
            }
            default:
            {
                return {false, "Unknown error"};
            }
        }
    }

    bool Core::getGlobalIndexesForRange(
        const uint64_t startHeight,
        const uint64_t endHeight,
        std::unordered_map<Crypto::Hash, std::vector<uint64_t>> &indexes) const
    {
        throwIfNotInitialized();

        try
        {
            IBlockchainCache *mainChain = chainsLeaves[0];

            std::vector<Crypto::Hash> transactionHashes;

            for (const auto rawBlock : mainChain->getBlocksByHeight(startHeight, endHeight))
            {
                for (const auto transaction : rawBlock.transactions)
                {
                    transactionHashes.push_back(getBinaryArrayHash(transaction));
                }

                BlockTemplate block;

                fromBinaryArray(block, rawBlock.block);

                transactionHashes.push_back(getBinaryArrayHash(toBinaryArray(block.baseTransaction)));
            }

            indexes = mainChain->getGlobalIndexes(transactionHashes);

            return true;
        }
        catch (std::exception &e)
        {
            logger(Logging::ERROR) << "Failed to get global indexes: " << e.what();
            return false;
        }
    }

    std::tuple<bool, std::string> Core::addTransactionToPool(const BinaryArray &transactionBinaryArray)
    {
        throwIfNotInitialized();

        Transaction transaction;
        if (!fromBinaryArray<Transaction>(transaction, transactionBinaryArray))
        {
            logger(Logging::WARNING) << "Couldn't add transaction to pool due to deserialization error";
            return {false, "Could not deserialize transaction"};
        }

        CachedTransaction cachedTransaction(std::move(transaction));
        auto transactionHash = cachedTransaction.getTransactionHash();

        const auto [success, error] = addTransactionToPool(std::move(cachedTransaction));
        if (!success)
        {
            return {false, error};
        }

        notifyObservers(makeAddTransactionMessage({transactionHash}));
        return {true, ""};
    }

    std::tuple<bool, std::string> Core::addTransactionToPool(CachedTransaction &&cachedTransaction)
    {
        TransactionValidatorState validatorState;

        auto transactionHash = cachedTransaction.getTransactionHash();

        /* If the transaction is already in the pool, then checking it again
           and/or trying to add it to the pool again wastes time and resources.
           We don't need to waste time doing this as everything we hear about
           from the network would result in us checking relayed transactions
           an insane number of times */
        if (transactionPool->checkIfTransactionPresent(transactionHash))
        {
            return {false, "Transaction already exists in pool"};
        }

        const auto [success, error] = isTransactionValidForPool(cachedTransaction, validatorState);
        if (!success)
        {
            return {false, error};
        }

        if (!transactionPool->pushTransaction(std::move(cachedTransaction), std::move(validatorState)))
        {
            logger(Logging::DEBUGGING) << "Failed to push transaction " << transactionHash
                                       << " to pool, already exists";
            return {false, "Transaction already exists in pool"};
        }

        logger(Logging::DEBUGGING) << "Transaction " << transactionHash << " has been added to pool";
        return {true, ""};
    }

    std::tuple<bool, std::string> Core::isTransactionValidForPool(
        const CachedTransaction &cachedTransaction,
        TransactionValidatorState &validatorState)
    {
        const auto transactionHash = cachedTransaction.getTransactionHash();

        /* If there are already a certain number of fusion transactions in
           the pool, then do not try to add another */
        if (cachedTransaction.getTransactionFee() == 0
            && transactionPool->getFusionTransactionCount() >= CryptoNote::parameters::FUSION_TX_MAX_POOL_COUNT)
        {
            return {false, "Pool already contains the maximum amount of fusion transactions"};
        }

        uint64_t fee;

        if (auto validationResult =
                validateTransaction(cachedTransaction, validatorState, chainsLeaves[0], m_transactionValidationThreadPool, fee, getTopBlockIndex(), true))
        {
            logger(Logging::DEBUGGING) << "Transaction " << transactionHash
                                       << " is not valid. Reason: " << validationResult.message();
            return {false, validationResult.message()};
        }

        return {true, ""};
    }

    std::vector<Crypto::Hash> Core::getPoolTransactionHashes() const
    {
        throwIfNotInitialized();

        return transactionPool->getTransactionHashes();
    }

    std::tuple<bool, CryptoNote::BinaryArray> Core::getPoolTransaction(const Crypto::Hash &transactionHash) const
    {
        if (transactionPool->checkIfTransactionPresent(transactionHash))
        {
            return {true, transactionPool->getTransaction(transactionHash).getTransactionBinaryArray()};
        }
        else
        {
            return {false, BinaryArray()};
        }
    }

    bool Core::getPoolChanges(
        const Crypto::Hash &lastBlockHash,
        const std::vector<Crypto::Hash> &knownHashes,
        std::vector<BinaryArray> &addedTransactions,
        std::vector<Crypto::Hash> &deletedTransactions) const
    {
        throwIfNotInitialized();

        std::vector<Crypto::Hash> newTransactions;
        getTransactionPoolDifference(knownHashes, newTransactions, deletedTransactions);

        addedTransactions.reserve(newTransactions.size());
        for (const auto &hash : newTransactions)
        {
            addedTransactions.emplace_back(transactionPool->getTransaction(hash).getTransactionBinaryArray());
        }

        return getTopBlockHash() == lastBlockHash;
    }

    bool Core::getPoolChangesLite(
        const Crypto::Hash &lastBlockHash,
        const std::vector<Crypto::Hash> &knownHashes,
        std::vector<TransactionPrefixInfo> &addedTransactions,
        std::vector<Crypto::Hash> &deletedTransactions) const
    {
        throwIfNotInitialized();

        std::vector<Crypto::Hash> newTransactions;
        getTransactionPoolDifference(knownHashes, newTransactions, deletedTransactions);

        addedTransactions.reserve(newTransactions.size());
        for (const auto &hash : newTransactions)
        {
            TransactionPrefixInfo transactionPrefixInfo;
            transactionPrefixInfo.txHash = hash;
            transactionPrefixInfo.txPrefix =
                static_cast<const TransactionPrefix &>(transactionPool->getTransaction(hash).getTransaction());
            addedTransactions.emplace_back(std::move(transactionPrefixInfo));
        }

        return getTopBlockHash() == lastBlockHash;
    }

    std::tuple<bool, std::string> Core::getBlockTemplate(
        BlockTemplate &b,
        const Crypto::PublicKey &publicViewKey,
        const Crypto::PublicKey &publicSpendKey,
        const BinaryArray &extraNonce,
        uint64_t &difficulty,
        uint32_t &height)
    {
        throwIfNotInitialized();

        height = getTopBlockIndex() + 1;
        difficulty = getDifficultyForNextBlock();

        if (difficulty == 0)
        {
            std::string error = "Cannot create block template, difficulty is zero. Oh shit, you fucked up hard!";

            logger(Logging::ERROR, Logging::BRIGHT_RED) << error;

            return {false, error};
        }

        b = boost::value_initialized<BlockTemplate>();
        b.majorVersion = getBlockMajorVersionForHeight(height);

        if (b.majorVersion == BLOCK_MAJOR_VERSION_1)
        {
            b.minorVersion = currency.upgradeHeight(BLOCK_MAJOR_VERSION_2) == IUpgradeDetector::UNDEF_HEIGHT
                                 ? BLOCK_MINOR_VERSION_1
                                 : BLOCK_MINOR_VERSION_0;
        }
        else if (b.majorVersion >= BLOCK_MAJOR_VERSION_2)
        {
            if (currency.upgradeHeight(BLOCK_MAJOR_VERSION_3) == IUpgradeDetector::UNDEF_HEIGHT)
            {
                b.minorVersion =
                    b.majorVersion == BLOCK_MAJOR_VERSION_2 ? BLOCK_MINOR_VERSION_1 : BLOCK_MINOR_VERSION_0;
            }
            else
            {
                b.minorVersion = BLOCK_MINOR_VERSION_0;
            }

            b.parentBlock.majorVersion = BLOCK_MAJOR_VERSION_1;
            b.parentBlock.majorVersion = BLOCK_MINOR_VERSION_0;
            b.parentBlock.transactionCount = 1;

            TransactionExtraMergeMiningTag mmTag = boost::value_initialized<decltype(mmTag)>();

            if (!appendMergeMiningTagToExtra(b.parentBlock.baseTransaction.extra, mmTag))
            {
                std::string error = "Failed to append merge mining tag to extra of the parent block miner transaction";

                logger(Logging::ERROR, Logging::BRIGHT_RED) << error;

                return {false, error};
            }
        }

        b.previousBlockHash = getTopBlockHash();
        b.timestamp = time(nullptr);

        /* Ok, so if an attacker is fiddling around with timestamps on the network,
           they can make it so all the valid pools / miners don't produce valid
           blocks. This is because the timestamp is created as the users current time,
           however, if the attacker is a large % of the hashrate, they can slowly
           increase the timestamp into the future, shifting the median timestamp
           forwards. At some point, this will mean the valid pools will submit a
           block with their valid timestamps, and it will be rejected for being
           behind the median timestamp / too far in the past. The simple way to
           handle this is just to check if our timestamp is going to be invalid, and
           set it to the median.

           Once the attack ends, the median timestamp will remain how it is, until
           the time on the clock goes forwards, and we can start submitting valid
           timestamps again, and then we are back to normal. */

        /* Thanks to jagerman for this patch:
           https://github.com/loki-project/loki/pull/26 */

        /* How many blocks we look in the past to calculate the median timestamp */
        uint64_t blockchain_timestamp_check_window;

        if (height >= CryptoNote::parameters::LWMA_2_DIFFICULTY_BLOCK_INDEX)
        {
            blockchain_timestamp_check_window = CryptoNote::parameters::BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V3;
        }
        else
        {
            blockchain_timestamp_check_window = CryptoNote::parameters::BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW;
        }

        /* Skip the first N blocks, we don't have enough blocks to calculate a
           proper median yet */
        if (height >= blockchain_timestamp_check_window)
        {
            std::vector<uint64_t> timestamps;

            /* For the last N blocks, get their timestamps */
            for (size_t offset = height - blockchain_timestamp_check_window; offset < height; offset++)
            {
                timestamps.push_back(getBlockTimestampByIndex(offset));
            }

            uint64_t medianTimestamp = Common::medianValue(timestamps);

            if (b.timestamp < medianTimestamp)
            {
                b.timestamp = medianTimestamp;
            }
        }

        size_t medianSize = calculateCumulativeBlocksizeLimit(height) / 2;

        assert(!chainsStorage.empty());
        assert(!chainsLeaves.empty());
        uint64_t alreadyGeneratedCoins = chainsLeaves[0]->getAlreadyGeneratedCoins();

        size_t transactionsSize;
        uint64_t fee;
        fillBlockTemplate(b, medianSize, currency.maxBlockCumulativeSize(height), height, transactionsSize, fee);

        /*
           two-phase miner transaction generation: we don't know exact block size until we prepare block, but we don't know
           reward until we know
           block size, so first miner transaction generated with fake amount of money, and with phase we know think we know
           expected block size
        */
        // make blocks coin-base tx looks close to real coinbase tx to get truthful blob size
        bool r = currency.constructMinerTx(
            b.majorVersion,
            height,
            medianSize,
            alreadyGeneratedCoins,
            transactionsSize,
            fee,
            publicViewKey,
            publicSpendKey,
            b.baseTransaction,
            extraNonce,
            11);

        if (!r)
        {
            std::string error = "Failed to construct miner transaction";

            logger(Logging::ERROR, Logging::BRIGHT_RED) << error;

            return {false, error};
        }

        size_t cumulativeSize = transactionsSize + getObjectBinarySize(b.baseTransaction);
        const size_t TRIES_COUNT = 10;
        for (size_t tryCount = 0; tryCount < TRIES_COUNT; ++tryCount)
        {
            r = currency.constructMinerTx(
                b.majorVersion,
                height,
                medianSize,
                alreadyGeneratedCoins,
                cumulativeSize,
                fee,
                publicViewKey,
                publicSpendKey,
                b.baseTransaction,
                extraNonce,
                11);

            if (!r)
            {
                std::string error = "Failed to construct miner transaction";

                logger(Logging::ERROR, Logging::BRIGHT_RED) << error;

                return {false, error};
            }

            size_t coinbaseBlobSize = getObjectBinarySize(b.baseTransaction);
            if (coinbaseBlobSize > cumulativeSize - transactionsSize)
            {
                cumulativeSize = transactionsSize + coinbaseBlobSize;
                continue;
            }

            if (coinbaseBlobSize < cumulativeSize - transactionsSize)
            {
                size_t delta = cumulativeSize - transactionsSize - coinbaseBlobSize;
                b.baseTransaction.extra.insert(b.baseTransaction.extra.end(), delta, 0);
                // here  could be 1 byte difference, because of extra field counter is varint, and it can become from
                // 1-byte len to 2-bytes len.
                if (cumulativeSize != transactionsSize + getObjectBinarySize(b.baseTransaction))
                {
                    if (!(cumulativeSize + 1 == transactionsSize + getObjectBinarySize(b.baseTransaction)))
                    {
                        std::stringstream stream;

                        stream << "unexpected case: cumulative_size=" << cumulativeSize
                               << " + 1 is not equal txs_cumulative_size=" << transactionsSize
                               << " + get_object_blobsize(b.baseTransaction)=" << getObjectBinarySize(b.baseTransaction);

                        std::string error = stream.str();

                        logger(Logging::ERROR, Logging::BRIGHT_RED) << error;

                        return {false, error};
                    }

                    b.baseTransaction.extra.resize(b.baseTransaction.extra.size() - 1);
                    if (cumulativeSize != transactionsSize + getObjectBinarySize(b.baseTransaction))
                    {
                        // fuck, not lucky, -1 makes varint-counter size smaller, in that case we continue to grow with
                        // cumulative_size
                        logger(Logging::TRACE, Logging::BRIGHT_RED)
                            << "Miner tx creation have no luck with delta_extra size = " << delta << " and "
                            << delta - 1;
                        cumulativeSize += delta - 1;
                        continue;
                    }

                    logger(Logging::DEBUGGING, Logging::BRIGHT_GREEN)
                        << "Setting extra for block: " << b.baseTransaction.extra.size() << ", try_count=" << tryCount;
                }
            }
            if (!(cumulativeSize == transactionsSize + getObjectBinarySize(b.baseTransaction)))
            {
                std::stringstream stream;

                stream << "unexpected case: cumulative_size=" << cumulativeSize
                       << " is not equal txs_cumulative_size=" << transactionsSize
                       << " + get_object_blobsize(b.baseTransaction)=" << getObjectBinarySize(b.baseTransaction);

                std::string error = stream.str();

                logger(Logging::ERROR, Logging::BRIGHT_RED) << error;

                return {false, error};
            }

            return {true, std::string()};
        }

        std::string error = "Failed to create block template";

        logger(Logging::ERROR, Logging::BRIGHT_RED) << error;

        return {false, error};
    }

    CoreStatistics Core::getCoreStatistics() const
    {
        // TODO: implement it
        assert(false);
        CoreStatistics result;
        std::fill(reinterpret_cast<uint8_t *>(&result), reinterpret_cast<uint8_t *>(&result) + sizeof(result), 0);
        return result;
    }

    size_t Core::getPoolTransactionCount() const
    {
        throwIfNotInitialized();
        return transactionPool->getTransactionCount();
    }

    size_t Core::getBlockchainTransactionCount() const
    {
        throwIfNotInitialized();
        IBlockchainCache *mainChain = chainsLeaves[0];
        return mainChain->getTransactionCount();
    }

    size_t Core::getAlternativeBlockCount() const
    {
        throwIfNotInitialized();

        using Ptr = decltype(chainsStorage)::value_type;
        return std::accumulate(chainsStorage.begin(), chainsStorage.end(), size_t(0), [&](size_t sum, const Ptr &ptr) {
            return mainChainSet.count(ptr.get()) == 0 ? sum + ptr->getBlockCount() : sum;
        });
    }

    std::vector<Transaction> Core::getPoolTransactions() const
    {
        throwIfNotInitialized();

        std::vector<Transaction> transactions;
        auto hashes = transactionPool->getPoolTransactions();
        std::transform(
            std::begin(hashes), std::end(hashes), std::back_inserter(transactions), [&](const CachedTransaction &tx) {
                return tx.getTransaction();
            });
        return transactions;
    }

    bool Core::extractTransactions(
        const std::vector<BinaryArray> &rawTransactions,
        std::vector<CachedTransaction> &transactions,
        uint64_t &cumulativeSize)
    {
        try
        {
            for (auto &rawTransaction : rawTransactions)
            {
                if (rawTransaction.size() > currency.maxTxSize())
                {
                    logger(Logging::INFO) << "Raw transaction size " << rawTransaction.size() << " is too big.";
                    return false;
                }

                cumulativeSize += rawTransaction.size();
                transactions.emplace_back(rawTransaction);
            }
        }
        catch (std::runtime_error &e)
        {
            logger(Logging::INFO) << e.what();
            return false;
        }

        return true;
    }

    std::error_code Core::validateTransaction(
        const CachedTransaction &cachedTransaction,
        TransactionValidatorState &state,
        IBlockchainCache *cache,
        Utilities::ThreadPool<bool> &threadPool,
        uint64_t &fee,
        uint32_t blockIndex,
        const bool isPoolTransaction)
    {
        ValidateTransaction txValidator(
            cachedTransaction,
            state,
            cache,
            currency,
            checkpoints,
            threadPool,
            blockIndex,
            blockMedianSize,
            isPoolTransaction
        );

        const auto result = txValidator.validate();

        fee = result.fee;

        return result.errorCode;
    }

    uint32_t Core::findBlockchainSupplement(const std::vector<Crypto::Hash> &remoteBlockIds) const
    {
        /* Requester doesn't know anything about the chain yet */
        if (remoteBlockIds.empty())
        {
            return 0;
        }

        // TODO: check for genesis blocks match
        for (auto &hash : remoteBlockIds)
        {
            IBlockchainCache *blockchainSegment = findMainChainSegmentContainingBlock(hash);
            if (blockchainSegment != nullptr)
            {
                return blockchainSegment->getBlockIndex(hash);
            }
        }

        throw std::runtime_error("Genesis block hash was not found.");
    }

    std::vector<Crypto::Hash> CryptoNote::Core::getBlockHashes(uint32_t startBlockIndex, uint32_t maxCount) const
    {
        return chainsLeaves[0]->getBlockHashes(startBlockIndex, maxCount);
    }

    std::error_code Core::validateBlock(const CachedBlock &cachedBlock, IBlockchainCache *cache, uint64_t &minerReward)
    {
        const auto &block = cachedBlock.getBlock();
        auto previousBlockIndex = cache->getBlockIndex(block.previousBlockHash);
        // assert(block.previousBlockHash == cache->getBlockHash(previousBlockIndex));

        minerReward = 0;

        if (upgradeManager->getBlockMajorVersion(cachedBlock.getBlockIndex()) != block.majorVersion)
        {
            return error::BlockValidationError::WRONG_VERSION;
        }

        if (block.majorVersion >= BLOCK_MAJOR_VERSION_2)
        {
            if (block.majorVersion == BLOCK_MAJOR_VERSION_2 && block.parentBlock.majorVersion > BLOCK_MAJOR_VERSION_1)
            {
                logger(Logging::ERROR, Logging::BRIGHT_RED)
                    << "Parent block of block " << cachedBlock.getBlockHash()
                    << " has wrong major version: " << static_cast<int>(block.parentBlock.majorVersion) << ", at index "
                    << cachedBlock.getBlockIndex() << " expected version is "
                    << static_cast<int>(BLOCK_MAJOR_VERSION_1);
                return error::BlockValidationError::PARENT_BLOCK_WRONG_VERSION;
            }

            if (cachedBlock.getParentBlockBinaryArray(false).size() > 2048)
            {
                return error::BlockValidationError::PARENT_BLOCK_SIZE_TOO_BIG;
            }
        }

        if (block.timestamp > getAdjustedTime() + currency.blockFutureTimeLimit(previousBlockIndex + 1))
        {
            return error::BlockValidationError::TIMESTAMP_TOO_FAR_IN_FUTURE;
        }

        auto timestamps = cache->getLastTimestamps(
            currency.timestampCheckWindow(previousBlockIndex + 1), previousBlockIndex, addGenesisBlock);
        if (timestamps.size() >= currency.timestampCheckWindow(previousBlockIndex + 1))
        {
            auto median_ts = Common::medianValue(timestamps);
            if (block.timestamp < median_ts)
            {
                return error::BlockValidationError::TIMESTAMP_TOO_FAR_IN_PAST;
            }
        }

        if (block.baseTransaction.inputs.size() != 1)
        {
            return error::TransactionValidationError::INPUT_WRONG_COUNT;
        }

        if (block.baseTransaction.inputs[0].type() != typeid(BaseInput))
        {
            return error::TransactionValidationError::INPUT_UNEXPECTED_TYPE;
        }

        if (boost::get<BaseInput>(block.baseTransaction.inputs[0]).blockIndex != previousBlockIndex + 1)
        {
            return error::TransactionValidationError::BASE_INPUT_WRONG_BLOCK_INDEX;
        }

        if (!(block.baseTransaction.unlockTime == previousBlockIndex + 1 + currency.minedMoneyUnlockWindow()))
        {
            return error::TransactionValidationError::WRONG_TRANSACTION_UNLOCK_TIME;
        }

        if (cachedBlock.getBlockIndex() >= CryptoNote::parameters::TRANSACTION_SIGNATURE_COUNT_VALIDATION_HEIGHT
            && !block.baseTransaction.signatures.empty())
        {
            return error::TransactionValidationError::BASE_INVALID_SIGNATURES_COUNT;
        }

        for (const auto &output : block.baseTransaction.outputs)
        {
            if (output.amount == 0)
            {
                return error::TransactionValidationError::OUTPUT_ZERO_AMOUNT;
            }

            if (output.target.type() == typeid(KeyOutput))
            {
                if (!check_key(boost::get<KeyOutput>(output.target).key))
                {
                    return error::TransactionValidationError::OUTPUT_INVALID_KEY;
                }
            }
            else
            {
                return error::TransactionValidationError::OUTPUT_UNKNOWN_TYPE;
            }

            if (std::numeric_limits<uint64_t>::max() - output.amount < minerReward)
            {
                return error::TransactionValidationError::OUTPUTS_AMOUNT_OVERFLOW;
            }

            minerReward += output.amount;
        }

        return error::BlockValidationError::VALIDATION_SUCCESS;
    }

    uint64_t CryptoNote::Core::getAdjustedTime() const
    {
        return time(NULL);
    }

    const Currency &Core::getCurrency() const
    {
        return currency;
    }

    void Core::save()
    {
        throwIfNotInitialized();

        deleteAlternativeChains();
        mergeMainChainSegments();
        chainsLeaves[0]->save();
    }

    void Core::load()
    {
        initRootSegment();

        start_time = std::time(nullptr);

        auto dbBlocksCount = chainsLeaves[0]->getTopBlockIndex() + 1;
        auto storageBlocksCount = mainChainStorage->getBlockCount();

        logger(Logging::DEBUGGING) << "Blockchain storage blocks count: " << storageBlocksCount
                                   << ", DB blocks count: " << dbBlocksCount;

        assert(storageBlocksCount != 0); // we assume the storage has at least genesis block

        if (storageBlocksCount > dbBlocksCount)
        {
            logger(Logging::INFO) << "Importing blocks from blockchain storage";
            importBlocksFromStorage();
        }
        else if (storageBlocksCount < dbBlocksCount)
        {
            auto cutFrom = findCommonRoot(*mainChainStorage, *chainsLeaves[0]) + 1;

            logger(Logging::INFO) << "DB has more blocks than blockchain storage, cutting from block index: "
                                  << cutFrom;
            cutSegment(*chainsLeaves[0], cutFrom);

            assert(chainsLeaves[0]->getTopBlockIndex() + 1 == mainChainStorage->getBlockCount());
        }
        else if (
            getBlockHash(mainChainStorage->getBlockByIndex(storageBlocksCount - 1))
            != chainsLeaves[0]->getTopBlockHash())
        {
            logger(Logging::INFO) << "Blockchain storage and root segment are on different chains. "
                                  << "Cutting root segment to common block index "
                                  << findCommonRoot(*mainChainStorage, *chainsLeaves[0]) << " and reimporting blocks";
            importBlocksFromStorage();
        }
        else
        {
            logger(Logging::DEBUGGING) << "Blockchain storage and root segment are on the same height and chain";
        }

        initialized = true;
    }

    void Core::initRootSegment()
    {
        std::unique_ptr<IBlockchainCache> cache = this->blockchainCacheFactory->createRootBlockchainCache(currency);

        mainChainSet.emplace(cache.get());

        chainsLeaves.push_back(cache.get());
        chainsStorage.push_back(std::move(cache));

        contextGroup.spawn(std::bind(&Core::transactionPoolCleaningProcedure, this));

        updateBlockMedianSize();

        chainsLeaves[0]->load();
    }

    void Core::importBlocksFromStorage()
    {
        uint32_t commonIndex = findCommonRoot(*mainChainStorage, *chainsLeaves[0]);
        assert(commonIndex <= mainChainStorage->getBlockCount());

        cutSegment(*chainsLeaves[0], commonIndex + 1);

        auto previousBlockHash = getBlockHash(mainChainStorage->getBlockByIndex(commonIndex));
        auto blockCount = mainChainStorage->getBlockCount();
        for (uint32_t i = commonIndex + 1; i < blockCount; ++i)
        {
            RawBlock rawBlock = mainChainStorage->getBlockByIndex(i);
            auto blockTemplate = extractBlockTemplate(rawBlock);
            CachedBlock cachedBlock(blockTemplate);

            if (blockTemplate.previousBlockHash != previousBlockHash)
            {
                logger(Logging::ERROR)
                    << "Local blockchain corruption detected. " << std::endl
                    << "Block with index " << i << " and hash " << cachedBlock.getBlockHash()
                    << " has previous block hash " << blockTemplate.previousBlockHash << ", but parent has hash "
                    << previousBlockHash << "." << std::endl
                    << "Please try to repair this issue by starting the node with the option: --rewind-to-height " << i
                    << std::endl
                    << "If the above does not repair the issue, please launch the node with the option: --resync"
                    << std::endl;
                throw std::system_error(make_error_code(error::CoreErrorCode::CORRUPTED_BLOCKCHAIN));
            }

            previousBlockHash = cachedBlock.getBlockHash();

            std::vector<CachedTransaction> transactions;
            uint64_t cumulativeSize = 0;
            if (!extractTransactions(rawBlock.transactions, transactions, cumulativeSize))
            {
                logger(Logging::ERROR) << "Couldn't deserialize raw block transactions in block "
                                       << cachedBlock.getBlockHash();
                throw std::system_error(make_error_code(error::AddBlockErrorCode::DESERIALIZATION_FAILED));
            }

            cumulativeSize += getObjectBinarySize(blockTemplate.baseTransaction);
            TransactionValidatorState spentOutputs = extractSpentOutputs(transactions);
            auto currentDifficulty = chainsLeaves[0]->getDifficultyForNextBlock(i - 1);

            uint64_t cumulativeFee = std::accumulate(
                transactions.begin(),
                transactions.end(),
                UINT64_C(0),
                [](uint64_t fee, const CachedTransaction &transaction) {
                    return fee + transaction.getTransactionFee();
                });

            int64_t emissionChange =
                getEmissionChange(currency, *chainsLeaves[0], i - 1, cachedBlock, cumulativeSize, cumulativeFee);
            chainsLeaves[0]->pushBlock(
                cachedBlock,
                transactions,
                spentOutputs,
                cumulativeSize,
                emissionChange,
                currentDifficulty,
                std::move(rawBlock));

            if (i % 1000 == 0)
            {
                logger(Logging::INFO) << "Imported block with index " << i << " / " << (blockCount - 1);
            }
        }
    }

    void Core::cutSegment(IBlockchainCache &segment, uint32_t startIndex)
    {
        if (segment.getTopBlockIndex() < startIndex)
        {
            return;
        }

        logger(Logging::INFO) << "Cutting root segment from index " << startIndex;
        auto childCache = segment.split(startIndex);
        segment.deleteChild(childCache.get());
    }

    void Core::updateMainChainSet()
    {
        mainChainSet.clear();
        IBlockchainCache *chainPtr = chainsLeaves[0];
        assert(chainPtr != nullptr);
        do
        {
            mainChainSet.insert(chainPtr);
            chainPtr = chainPtr->getParent();
        } while (chainPtr != nullptr);
    }

    IBlockchainCache *Core::findSegmentContainingBlock(const Crypto::Hash &blockHash) const
    {
        assert(chainsLeaves.size() > 0);

        // first search in main chain
        auto blockSegment = findMainChainSegmentContainingBlock(blockHash);
        if (blockSegment != nullptr)
        {
            return blockSegment;
        }

        // than search in alternative chains
        return findAlternativeSegmentContainingBlock(blockHash);
    }

    IBlockchainCache *Core::findSegmentContainingBlock(uint32_t blockHeight) const
    {
        assert(chainsLeaves.size() > 0);

        // first search in main chain
        auto blockSegment = findMainChainSegmentContainingBlock(blockHeight);
        if (blockSegment != nullptr)
        {
            return blockSegment;
        }

        // than search in alternative chains
        return findAlternativeSegmentContainingBlock(blockHeight);
    }

    IBlockchainCache *Core::findAlternativeSegmentContainingBlock(const Crypto::Hash &blockHash) const
    {
        IBlockchainCache *cache = nullptr;
        std::find_if(++chainsLeaves.begin(), chainsLeaves.end(), [&](IBlockchainCache *chain) {
            return cache = findIndexInChain(chain, blockHash);
        });
        return cache;
    }

    IBlockchainCache *Core::findMainChainSegmentContainingBlock(const Crypto::Hash &blockHash) const
    {
        return findIndexInChain(chainsLeaves[0], blockHash);
    }

    IBlockchainCache *Core::findMainChainSegmentContainingBlock(uint32_t blockIndex) const
    {
        return findIndexInChain(chainsLeaves[0], blockIndex);
    }

    // WTF?! this function returns first chain it is able to find..
    IBlockchainCache *Core::findAlternativeSegmentContainingBlock(uint32_t blockIndex) const
    {
        IBlockchainCache *cache = nullptr;
        std::find_if(++chainsLeaves.begin(), chainsLeaves.end(), [&](IBlockchainCache *chain) {
            return cache = findIndexInChain(chain, blockIndex);
        });
        return nullptr;
    }

    BlockTemplate Core::restoreBlockTemplate(IBlockchainCache *blockchainCache, uint32_t blockIndex) const
    {
        RawBlock rawBlock = blockchainCache->getBlockByIndex(blockIndex);

        BlockTemplate block;
        if (!fromBinaryArray(block, rawBlock.block))
        {
            throw std::runtime_error("Coulnd't deserialize BlockTemplate");
        }

        return block;
    }

    std::vector<Crypto::Hash> Core::doBuildSparseChain(const Crypto::Hash &blockHash) const
    {
        IBlockchainCache *chain = findSegmentContainingBlock(blockHash);

        uint32_t blockIndex = chain->getBlockIndex(blockHash);

        // TODO reserve ceil(log(blockIndex))
        std::vector<Crypto::Hash> sparseChain;
        sparseChain.push_back(blockHash);

        for (uint32_t i = 1; i < blockIndex; i *= 2)
        {
            sparseChain.push_back(chain->getBlockHash(blockIndex - i));
        }

        auto genesisBlockHash = chain->getBlockHash(0);
        if (sparseChain[0] != genesisBlockHash)
        {
            sparseChain.push_back(genesisBlockHash);
        }

        return sparseChain;
    }

    RawBlock Core::getRawBlock(IBlockchainCache *segment, uint32_t blockIndex) const
    {
        assert(blockIndex >= segment->getStartBlockIndex() && blockIndex <= segment->getTopBlockIndex());

        return segment->getBlockByIndex(blockIndex);
    }

    // TODO: decompose these three methods
    size_t Core::pushBlockHashes(
        uint32_t startIndex,
        uint32_t fullOffset,
        size_t maxItemsCount,
        std::vector<BlockShortInfo> &entries) const
    {
        assert(fullOffset >= startIndex);
        uint32_t itemsCount = std::min(fullOffset - startIndex, static_cast<uint32_t>(maxItemsCount));
        if (itemsCount == 0)
        {
            return 0;
        }
        std::vector<Crypto::Hash> blockIds = getBlockHashes(startIndex, itemsCount);
        entries.reserve(entries.size() + blockIds.size());
        for (auto &blockHash : blockIds)
        {
            BlockShortInfo entry;
            entry.blockId = std::move(blockHash);
            entries.emplace_back(std::move(entry));
        }
        return blockIds.size();
    }

    // TODO: decompose these three methods
    size_t Core::pushBlockHashes(
        uint32_t startIndex,
        uint32_t fullOffset,
        size_t maxItemsCount,
        std::vector<BlockDetails> &entries) const
    {
        assert(fullOffset >= startIndex);
        uint32_t itemsCount = std::min(fullOffset - startIndex, static_cast<uint32_t>(maxItemsCount));
        if (itemsCount == 0)
        {
            return 0;
        }
        std::vector<Crypto::Hash> blockIds = getBlockHashes(startIndex, itemsCount);
        entries.reserve(entries.size() + blockIds.size());
        for (auto &blockHash : blockIds)
        {
            BlockDetails entry;
            entry.hash = std::move(blockHash);
            entries.emplace_back(std::move(entry));
        }
        return blockIds.size();
    }

    // TODO: decompose these three methods
    size_t Core::pushBlockHashes(
        uint32_t startIndex,
        uint32_t fullOffset,
        size_t maxItemsCount,
        std::vector<BlockFullInfo> &entries) const
    {
        assert(fullOffset >= startIndex);
        uint32_t itemsCount = std::min(fullOffset - startIndex, static_cast<uint32_t>(maxItemsCount));
        if (itemsCount == 0)
        {
            return 0;
        }
        std::vector<Crypto::Hash> blockIds = getBlockHashes(startIndex, itemsCount);
        entries.reserve(entries.size() + blockIds.size());
        for (auto &blockHash : blockIds)
        {
            BlockFullInfo entry;
            entry.block_id = std::move(blockHash);
            entries.emplace_back(std::move(entry));
        }
        return blockIds.size();
    }

    void Core::fillQueryBlockFullInfo(
        uint32_t fullOffset,
        uint32_t currentIndex,
        size_t maxItemsCount,
        std::vector<BlockFullInfo> &entries) const
    {
        assert(currentIndex >= fullOffset);

        uint32_t fullBlocksCount =
            static_cast<uint32_t>(std::min(static_cast<uint32_t>(maxItemsCount), currentIndex - fullOffset));
        entries.reserve(entries.size() + fullBlocksCount);

        for (uint32_t blockIndex = fullOffset; blockIndex < fullOffset + fullBlocksCount; ++blockIndex)
        {
            IBlockchainCache *segment = findMainChainSegmentContainingBlock(blockIndex);

            BlockFullInfo blockFullInfo;
            blockFullInfo.block_id = segment->getBlockHash(blockIndex);
            static_cast<RawBlock &>(blockFullInfo) = getRawBlock(segment, blockIndex);

            entries.emplace_back(std::move(blockFullInfo));
        }
    }

    void Core::fillQueryBlockShortInfo(
        uint32_t fullOffset,
        uint32_t currentIndex,
        size_t maxItemsCount,
        std::vector<BlockShortInfo> &entries) const
    {
        assert(currentIndex >= fullOffset);

        uint32_t fullBlocksCount =
            static_cast<uint32_t>(std::min(static_cast<uint32_t>(maxItemsCount), currentIndex - fullOffset + 1));
        entries.reserve(entries.size() + fullBlocksCount);

        for (uint32_t blockIndex = fullOffset; blockIndex < fullOffset + fullBlocksCount; ++blockIndex)
        {
            IBlockchainCache *segment = findMainChainSegmentContainingBlock(blockIndex);
            RawBlock rawBlock = getRawBlock(segment, blockIndex);

            BlockShortInfo blockShortInfo;
            blockShortInfo.block = std::move(rawBlock.block);
            blockShortInfo.blockId = segment->getBlockHash(blockIndex);

            blockShortInfo.txPrefixes.reserve(rawBlock.transactions.size());
            for (auto &rawTransaction : rawBlock.transactions)
            {
                TransactionPrefixInfo prefixInfo;
                prefixInfo.txHash =
                    getBinaryArrayHash(rawTransaction); // TODO: is there faster way to get hash without calculation?

                Transaction transaction;
                if (!fromBinaryArray(transaction, rawTransaction))
                {
                    // TODO: log it
                    throw std::runtime_error("Couldn't deserialize transaction");
                }

                prefixInfo.txPrefix = std::move(static_cast<TransactionPrefix &>(transaction));
                blockShortInfo.txPrefixes.emplace_back(std::move(prefixInfo));
            }

            entries.emplace_back(std::move(blockShortInfo));
        }
    }

    void Core::fillQueryBlockDetails(
        uint32_t fullOffset,
        uint32_t currentIndex,
        size_t maxItemsCount,
        std::vector<BlockDetails> &entries) const
    {
        assert(currentIndex >= fullOffset);

        uint32_t fullBlocksCount =
            static_cast<uint32_t>(std::min(static_cast<uint32_t>(maxItemsCount), currentIndex - fullOffset + 1));
        entries.reserve(entries.size() + fullBlocksCount);

        for (uint32_t blockIndex = fullOffset; blockIndex < fullOffset + fullBlocksCount; ++blockIndex)
        {
            IBlockchainCache *segment = findMainChainSegmentContainingBlock(blockIndex);
            Crypto::Hash blockHash = segment->getBlockHash(blockIndex);
            BlockDetails block = getBlockDetails(blockHash);
            entries.emplace_back(std::move(block));
        }
    }

    void Core::getTransactionPoolDifference(
        const std::vector<Crypto::Hash> &knownHashes,
        std::vector<Crypto::Hash> &newTransactions,
        std::vector<Crypto::Hash> &deletedTransactions) const
    {
        auto t = transactionPool->getTransactionHashes();

        std::unordered_set<Crypto::Hash> poolTransactions(t.begin(), t.end());
        std::unordered_set<Crypto::Hash> knownTransactions(knownHashes.begin(), knownHashes.end());

        for (auto it = poolTransactions.begin(), end = poolTransactions.end(); it != end;)
        {
            auto knownTransactionIt = knownTransactions.find(*it);
            if (knownTransactionIt != knownTransactions.end())
            {
                knownTransactions.erase(knownTransactionIt);
                it = poolTransactions.erase(it);
            }
            else
            {
                ++it;
            }
        }

        newTransactions.assign(poolTransactions.begin(), poolTransactions.end());
        deletedTransactions.assign(knownTransactions.begin(), knownTransactions.end());
    }

    uint8_t Core::getBlockMajorVersionForHeight(uint32_t height) const
    {
        return upgradeManager->getBlockMajorVersion(height);
    }

    size_t Core::calculateCumulativeBlocksizeLimit(uint32_t height) const
    {
        uint8_t nextBlockMajorVersion = getBlockMajorVersionForHeight(height);
        size_t nextBlockGrantedFullRewardZone =
            currency.blockGrantedFullRewardZoneByBlockVersion(nextBlockMajorVersion);

        assert(!chainsStorage.empty());
        assert(!chainsLeaves.empty());
        // FIXME: skip gensis here?
        auto sizes = chainsLeaves[0]->getLastBlocksSizes(currency.rewardBlocksWindow());
        uint64_t median = Common::medianValue(sizes);
        if (median <= nextBlockGrantedFullRewardZone)
        {
            median = nextBlockGrantedFullRewardZone;
        }

        return median * 2;
    }

    /* A transaction that is valid at the time it was added to the pool, is not
       neccessarily valid now, if the network rules changed. */
    bool Core::validateBlockTemplateTransaction(const CachedTransaction &cachedTransaction, const uint64_t blockHeight)
    {
        /* Not used in revalidateAfterHeightChange() */
        TransactionValidatorState state;

        ValidateTransaction txValidator(
            cachedTransaction,
            state,
            nullptr, /* Not used in revalidateAfterHeightChange() */
            currency,
            checkpoints,
            m_transactionValidationThreadPool,
            blockHeight,
            blockMedianSize,
            true /* Pool transaction */
        );

        const auto result = txValidator.revalidateAfterHeightChange();

        return result.valid;
    }

    void Core::fillBlockTemplate(
        BlockTemplate &block,
        const size_t medianSize,
        const size_t maxCumulativeSize,
        const uint64_t height,
        size_t &transactionsSize,
        uint64_t &fee)
    {
        transactionsSize = 0;
        fee = 0;

        size_t maxTotalSize = (125 * medianSize) / 100;

        maxTotalSize = std::min(maxTotalSize, maxCumulativeSize) - currency.minerTxBlobReservedSize();

        TransactionSpentInputsChecker spentInputsChecker;

        /* Go get our regular and fusion transactions from the transaction pool */
        auto [regularTransactions, fusionTransactions] = transactionPool->getPoolTransactionsForBlockTemplate();

        /* Define our lambda function for checking and adding transactions to a block template */
        const auto addTransactionToBlockTemplate =
            [this, &spentInputsChecker, maxTotalSize, height, &transactionsSize, &fee, &block](
                const CachedTransaction &transaction) {
                /* If the current set of transactions included in the blocktemplate plus the transaction
                   we just passed in exceed the maximum size of a block, it won't fit so we'll move on */
                if (transactionsSize + transaction.getTransactionBinaryArray().size() > maxTotalSize)
                {
                    return false;
                }

                /* Check to validate that the transaction is valid for a block at this height */
                if (!validateBlockTemplateTransaction(transaction, height))
                {
                    transactionPool->removeTransaction(transaction.getTransactionHash());

                    return false;
                }

                /* Make sure that we have not already spent funds in this same block via
                   another transaction that we've already included in this block template */
                if (!spentInputsChecker.haveSpentInputs(transaction.getTransaction()))
                {
                    transactionsSize += transaction.getTransactionBinaryArray().size();

                    fee += transaction.getTransactionFee();

                    block.transactionHashes.emplace_back(transaction.getTransactionHash());

                    return true;
                }
                else
                {
                    return false;
                }
            };

        /* First we're going to loop through transactions that have a fee:
           ie. the transactions that are paying to use the network */
        for (const auto &transaction : regularTransactions)
        {
            if (addTransactionToBlockTemplate(transaction))
            {
                logger(Logging::TRACE) << "Transaction " << transaction.getTransactionHash()
                                       << " included in block template";
            }
            else
            {
                logger(Logging::TRACE) << "Transaction " << transaction.getTransactionHash()
                                       << " not included in block template";
            }
        }

        /* Then we'll loop through the fusion transactions as they don't
           pay anything to use the network */
        for (const auto &transaction : fusionTransactions)
        {
            if (addTransactionToBlockTemplate(transaction))
            {
                logger(Logging::TRACE) << "Fusion transaction " << transaction.getTransactionHash()
                                       << " included in block template";
            }
        }
    }

    void Core::deleteAlternativeChains()
    {
        while (chainsLeaves.size() > 1)
        {
            deleteLeaf(1);
        }
    }

    void Core::deleteLeaf(size_t leafIndex)
    {
        assert(leafIndex < chainsLeaves.size());

        IBlockchainCache *leaf = chainsLeaves[leafIndex];

        IBlockchainCache *parent = leaf->getParent();
        if (parent != nullptr)
        {
            bool r = parent->deleteChild(leaf);
            if (r)
            {
            }
            assert(r);
        }

        auto segmentIt = std::find_if(
            chainsStorage.begin(), chainsStorage.end(), [&leaf](const std::unique_ptr<IBlockchainCache> &segment) {
                return segment.get() == leaf;
            });

        assert(segmentIt != chainsStorage.end());

        if (leafIndex != 0)
        {
            if (parent->getChildCount() == 0)
            {
                chainsLeaves.push_back(parent);
            }

            chainsLeaves.erase(chainsLeaves.begin() + leafIndex);
        }
        else
        {
            if (parent != nullptr)
            {
                chainsLeaves[0] = parent;
            }
            else
            {
                chainsLeaves.erase(chainsLeaves.begin());
            }
        }

        chainsStorage.erase(segmentIt);
    }

    void Core::mergeMainChainSegments()
    {
        assert(!chainsStorage.empty());
        assert(!chainsLeaves.empty());

        std::vector<IBlockchainCache *> chain;
        IBlockchainCache *segment = chainsLeaves[0];
        while (segment != nullptr)
        {
            chain.push_back(segment);
            segment = segment->getParent();
        }

        IBlockchainCache *rootSegment = chain.back();
        for (auto it = ++chain.rbegin(); it != chain.rend(); ++it)
        {
            mergeSegments(rootSegment, *it);
        }

        auto rootIt = std::find_if(
            chainsStorage.begin(),
            chainsStorage.end(),
            [&rootSegment](const std::unique_ptr<IBlockchainCache> &segment) { return segment.get() == rootSegment; });

        assert(rootIt != chainsStorage.end());

        if (rootIt != chainsStorage.begin())
        {
            *chainsStorage.begin() = std::move(*rootIt);
        }

        chainsStorage.erase(++chainsStorage.begin(), chainsStorage.end());
        chainsLeaves.clear();
        chainsLeaves.push_back(chainsStorage.begin()->get());
    }

    void Core::mergeSegments(IBlockchainCache *acceptingSegment, IBlockchainCache *segment)
    {
        assert(
            segment->getStartBlockIndex()
            == acceptingSegment->getStartBlockIndex() + acceptingSegment->getBlockCount());

        auto startIndex = segment->getStartBlockIndex();
        auto blockCount = segment->getBlockCount();
        for (auto blockIndex = startIndex; blockIndex < startIndex + blockCount; ++blockIndex)
        {
            PushedBlockInfo info = segment->getPushedBlockInfo(blockIndex);

            BlockTemplate block;
            if (!fromBinaryArray(block, info.rawBlock.block))
            {
                logger(Logging::WARNING) << "mergeSegments error: Couldn't deserialize block";
                throw std::runtime_error("Couldn't deserialize block");
            }

            std::vector<CachedTransaction> transactions;
            if (!Utils::restoreCachedTransactions(info.rawBlock.transactions, transactions))
            {
                logger(Logging::WARNING) << "mergeSegments error: Couldn't deserialize transactions";
                throw std::runtime_error("Couldn't deserialize transactions");
            }

            acceptingSegment->pushBlock(
                CachedBlock(block),
                transactions,
                info.validatorState,
                info.blockSize,
                info.generatedCoins,
                info.blockDifficulty,
                std::move(info.rawBlock));
        }
    }

    BlockDetails Core::getBlockDetails(const uint32_t blockHeight, const uint32_t attempt) const
    {
        if (attempt > 10)
        {
            throw std::runtime_error("Requested block height wasn't found in blockchain.");
        }

        throwIfNotInitialized();

        IBlockchainCache *segment = findSegmentContainingBlock(blockHeight);
        if (segment == nullptr)
        {
            throw std::runtime_error("Requested block height wasn't found in blockchain.");
        }

        try
        {
            return getBlockDetails(segment->getBlockHash(blockHeight));
        }
        catch (const std::out_of_range &e)
        {
            logger(Logging::INFO) << "Failed to get block details, mid chain reorg";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            return getBlockDetails(blockHeight, attempt+1);
        }
    }

    BlockDetails Core::getBlockDetails(const Crypto::Hash &blockHash) const
    {
        throwIfNotInitialized();

        IBlockchainCache *segment = findSegmentContainingBlock(blockHash);
        if (segment == nullptr)
        {
            throw std::runtime_error("Requested hash wasn't found in blockchain.");
        }

        uint32_t blockIndex = segment->getBlockIndex(blockHash);
        BlockTemplate blockTemplate = restoreBlockTemplate(segment, blockIndex);

        BlockDetails blockDetails;
        blockDetails.majorVersion = blockTemplate.majorVersion;
        blockDetails.minorVersion = blockTemplate.minorVersion;
        blockDetails.timestamp = blockTemplate.timestamp;
        blockDetails.prevBlockHash = blockTemplate.previousBlockHash;
        blockDetails.nonce = blockTemplate.nonce;
        blockDetails.hash = blockHash;

        blockDetails.reward = 0;
        for (const TransactionOutput &out : blockTemplate.baseTransaction.outputs)
        {
            blockDetails.reward += out.amount;
        }

        blockDetails.index = blockIndex;
        blockDetails.isAlternative = mainChainSet.count(segment) == 0;

        blockDetails.difficulty = getBlockDifficulty(blockIndex);

        std::vector<uint64_t> sizes = segment->getLastBlocksSizes(1, blockDetails.index, addGenesisBlock);
        assert(sizes.size() == 1);
        blockDetails.transactionsCumulativeSize = sizes.front();

        uint64_t blockBlobSize = getObjectBinarySize(blockTemplate);
        uint64_t coinbaseTransactionSize = getObjectBinarySize(blockTemplate.baseTransaction);
        blockDetails.blockSize = blockBlobSize + blockDetails.transactionsCumulativeSize - coinbaseTransactionSize;

        blockDetails.alreadyGeneratedCoins = segment->getAlreadyGeneratedCoins(blockDetails.index);
        blockDetails.alreadyGeneratedTransactions = segment->getAlreadyGeneratedTransactions(blockDetails.index);

        uint64_t prevBlockGeneratedCoins = 0;
        blockDetails.sizeMedian = 0;
        if (blockDetails.index > 0)
        {
            auto lastBlocksSizes =
                segment->getLastBlocksSizes(currency.rewardBlocksWindow(), blockDetails.index - 1, addGenesisBlock);
            blockDetails.sizeMedian = Common::medianValue(lastBlocksSizes);
            prevBlockGeneratedCoins = segment->getAlreadyGeneratedCoins(blockDetails.index - 1);
        }

        int64_t emissionChange = 0;
        bool result = currency.getBlockReward(
            blockDetails.majorVersion,
            blockDetails.sizeMedian,
            0,
            prevBlockGeneratedCoins,
            0,
            blockDetails.baseReward,
            emissionChange);
        if (result)
        {
        }
        assert(result);

        uint64_t currentReward = 0;
        result = currency.getBlockReward(
            blockDetails.majorVersion,
            blockDetails.sizeMedian,
            blockDetails.transactionsCumulativeSize,
            prevBlockGeneratedCoins,
            0,
            currentReward,
            emissionChange);
        assert(result);

        if (blockDetails.baseReward == 0 && currentReward == 0)
        {
            blockDetails.penalty = static_cast<double>(0);
        }
        else
        {
            assert(blockDetails.baseReward >= currentReward);
            blockDetails.penalty = static_cast<double>(blockDetails.baseReward - currentReward)
                                   / static_cast<double>(blockDetails.baseReward);
        }

        blockDetails.transactions.reserve(blockTemplate.transactionHashes.size() + 1);
        CachedTransaction cachedBaseTx(std::move(blockTemplate.baseTransaction));
        blockDetails.transactions.push_back(getTransactionDetails(cachedBaseTx.getTransactionHash(), segment, false));

        blockDetails.totalFeeAmount = 0;
        for (const Crypto::Hash &transactionHash : blockTemplate.transactionHashes)
        {
            blockDetails.transactions.push_back(getTransactionDetails(transactionHash, segment, false));
            blockDetails.totalFeeAmount += blockDetails.transactions.back().fee;
        }

        return blockDetails;
    }

    TransactionDetails Core::getTransactionDetails(const Crypto::Hash &transactionHash) const
    {
        throwIfNotInitialized();

        IBlockchainCache *segment = findSegmentContainingTransaction(transactionHash);
        bool foundInPool = transactionPool->checkIfTransactionPresent(transactionHash);
        if (segment == nullptr && !foundInPool)
        {
            throw std::runtime_error("Requested transaction wasn't found.");
        }

        return getTransactionDetails(transactionHash, segment, foundInPool);
    }

    TransactionDetails Core::getTransactionDetails(
        const Crypto::Hash &transactionHash,
        IBlockchainCache *segment,
        bool foundInPool) const
    {
        assert((segment != nullptr) != foundInPool);
        if (segment == nullptr)
        {
            segment = chainsLeaves[0];
        }

        std::unique_ptr<ITransaction> transaction;
        Transaction rawTransaction;
        TransactionDetails transactionDetails;
        if (!foundInPool)
        {
            std::vector<Crypto::Hash> transactionsHashes;
            std::vector<BinaryArray> rawTransactions;
            std::vector<Crypto::Hash> missedTransactionsHashes;
            transactionsHashes.push_back(transactionHash);

            segment->getRawTransactions(transactionsHashes, rawTransactions, missedTransactionsHashes);
            assert(missedTransactionsHashes.empty());
            assert(rawTransactions.size() == 1);

            std::vector<CachedTransaction> transactions;
            Utils::restoreCachedTransactions(rawTransactions, transactions);
            assert(transactions.size() == 1);

            transactionDetails.inBlockchain = true;
            transactionDetails.blockIndex = segment->getBlockIndexContainingTx(transactionHash);
            transactionDetails.blockHash = segment->getBlockHash(transactionDetails.blockIndex);

            auto timestamps = segment->getLastTimestamps(1, transactionDetails.blockIndex, addGenesisBlock);
            assert(timestamps.size() == 1);
            transactionDetails.timestamp = timestamps.back();

            transactionDetails.size = transactions.back().getTransactionBinaryArray().size();
            transactionDetails.fee = transactions.back().getTransactionFee();

            rawTransaction = transactions.back().getTransaction();
            transaction = createTransaction(rawTransaction);
        }
        else
        {
            transactionDetails.inBlockchain = false;
            transactionDetails.timestamp = transactionPool->getTransactionReceiveTime(transactionHash);

            transactionDetails.size =
                transactionPool->getTransaction(transactionHash).getTransactionBinaryArray().size();
            transactionDetails.fee = transactionPool->getTransaction(transactionHash).getTransactionFee();

            rawTransaction = transactionPool->getTransaction(transactionHash).getTransaction();
            transaction = createTransaction(rawTransaction);
        }

        transactionDetails.hash = transactionHash;
        transactionDetails.unlockTime = transaction->getUnlockTime();

        transactionDetails.totalOutputsAmount = transaction->getOutputTotalAmount();
        transactionDetails.totalInputsAmount = transaction->getInputTotalAmount();

        transactionDetails.mixin = 0;
        for (size_t i = 0; i < transaction->getInputCount(); ++i)
        {
            if (transaction->getInputType(i) != TransactionTypes::InputType::Key)
            {
                continue;
            }

            KeyInput input;
            transaction->getInput(i, input);
            uint64_t currentMixin = input.outputIndexes.size();
            if (currentMixin > transactionDetails.mixin)
            {
                transactionDetails.mixin = currentMixin;
            }
        }

        transactionDetails.paymentId = boost::value_initialized<Crypto::Hash>();
        if (transaction->getPaymentId(transactionDetails.paymentId))
        {
            transactionDetails.hasPaymentId = true;
        }
        transactionDetails.extra.publicKey = transaction->getTransactionPublicKey();
        transaction->getExtraNonce(transactionDetails.extra.nonce);
        transactionDetails.extra.raw = transaction->getExtra();

        transactionDetails.signatures = rawTransaction.signatures;

        transactionDetails.inputs.reserve(transaction->getInputCount());
        for (size_t i = 0; i < transaction->getInputCount(); ++i)
        {
            TransactionInputDetails txInDetails;

            if (transaction->getInputType(i) == TransactionTypes::InputType::Generating)
            {
                BaseInputDetails baseDetails;
                baseDetails.input = boost::get<BaseInput>(rawTransaction.inputs[i]);
                baseDetails.amount = transaction->getOutputTotalAmount();
                txInDetails = baseDetails;
            }
            else if (transaction->getInputType(i) == TransactionTypes::InputType::Key)
            {
                KeyInputDetails txInToKeyDetails;
                txInToKeyDetails.input = boost::get<KeyInput>(rawTransaction.inputs[i]);
                std::vector<std::pair<Crypto::Hash, size_t>> outputReferences;
                outputReferences.reserve(txInToKeyDetails.input.outputIndexes.size());
                std::vector<uint32_t> globalIndexes =
                    relativeOutputOffsetsToAbsolute(txInToKeyDetails.input.outputIndexes);
                ExtractOutputKeysResult result = segment->extractKeyOtputReferences(
                    txInToKeyDetails.input.amount, {globalIndexes.data(), globalIndexes.size()}, outputReferences);
                (void)result;
                assert(result == ExtractOutputKeysResult::SUCCESS);
                assert(txInToKeyDetails.input.outputIndexes.size() == outputReferences.size());

                txInToKeyDetails.mixin = txInToKeyDetails.input.outputIndexes.size();
                txInToKeyDetails.output.number = outputReferences.back().second;
                txInToKeyDetails.output.transactionHash = outputReferences.back().first;
                txInDetails = txInToKeyDetails;
            }

            assert(!txInDetails.empty());
            transactionDetails.inputs.push_back(std::move(txInDetails));
        }

        transactionDetails.outputs.reserve(transaction->getOutputCount());
        std::vector<uint32_t> globalIndexes;
        globalIndexes.reserve(transaction->getOutputCount());
        if (!transactionDetails.inBlockchain || !getTransactionGlobalIndexes(transactionDetails.hash, globalIndexes))
        {
            for (size_t i = 0; i < transaction->getOutputCount(); ++i)
            {
                globalIndexes.push_back(0);
            }
        }

        assert(transaction->getOutputCount() == globalIndexes.size());
        for (size_t i = 0; i < transaction->getOutputCount(); ++i)
        {
            TransactionOutputDetails txOutDetails;
            txOutDetails.output = rawTransaction.outputs[i];
            txOutDetails.globalIndex = globalIndexes[i];
            transactionDetails.outputs.push_back(std::move(txOutDetails));
        }

        return transactionDetails;
    }

    std::vector<Crypto::Hash> Core::getBlockHashesByTimestamps(uint64_t timestampBegin, size_t secondsCount) const
    {
        throwIfNotInitialized();

        logger(Logging::DEBUGGING) << "getBlockHashesByTimestamps request with timestamp " << timestampBegin
                                   << " and seconds count " << secondsCount;

        auto mainChain = chainsLeaves[0];

        if (timestampBegin + static_cast<uint64_t>(secondsCount) < timestampBegin)
        {
            logger(Logging::WARNING) << "Timestamp overflow occured. Timestamp begin: " << timestampBegin
                                     << ", timestamp end: " << (timestampBegin + static_cast<uint64_t>(secondsCount));

            throw std::runtime_error("Timestamp overflow");
        }

        return mainChain->getBlockHashesByTimestamps(timestampBegin, secondsCount);
    }

    std::vector<Crypto::Hash> Core::getTransactionHashesByPaymentId(const Hash &paymentId) const
    {
        throwIfNotInitialized();

        logger(Logging::DEBUGGING) << "getTransactionHashesByPaymentId request with paymentId " << paymentId;

        auto mainChain = chainsLeaves[0];

        std::vector<Crypto::Hash> hashes = mainChain->getTransactionHashesByPaymentId(paymentId);
        std::vector<Crypto::Hash> poolHashes = transactionPool->getTransactionHashesByPaymentId(paymentId);

        hashes.reserve(hashes.size() + poolHashes.size());
        std::move(poolHashes.begin(), poolHashes.end(), std::back_inserter(hashes));

        return hashes;
    }

    void Core::throwIfNotInitialized() const
    {
        if (!initialized)
        {
            throw std::system_error(make_error_code(error::CoreErrorCode::NOT_INITIALIZED));
        }
    }

    IBlockchainCache *Core::findSegmentContainingTransaction(const Crypto::Hash &transactionHash) const
    {
        assert(!chainsLeaves.empty());
        assert(!chainsStorage.empty());

        IBlockchainCache *segment = chainsLeaves[0];
        assert(segment != nullptr);

        // find in main chain
        do
        {
            if (segment->hasTransaction(transactionHash))
            {
                return segment;
            }

            segment = segment->getParent();
        } while (segment != nullptr);

        // find in alternative chains
        for (size_t chain = 1; chain < chainsLeaves.size(); ++chain)
        {
            segment = chainsLeaves[chain];

            while (mainChainSet.count(segment) == 0)
            {
                if (segment->hasTransaction(transactionHash))
                {
                    return segment;
                }

                segment = segment->getParent();
            }
        }

        return nullptr;
    }

    bool Core::hasTransaction(const Crypto::Hash &transactionHash) const
    {
        throwIfNotInitialized();
        return findSegmentContainingTransaction(transactionHash) != nullptr
               || transactionPool->checkIfTransactionPresent(transactionHash);
    }

    void Core::transactionPoolCleaningProcedure()
    {
        System::Timer timer(dispatcher);

        try
        {
            for (;;)
            {
                timer.sleep(OUTDATED_TRANSACTION_POLLING_INTERVAL);

                auto deletedTransactions = transactionPool->clean(getTopBlockIndex());
                notifyObservers(makeDelTransactionMessage(
                    std::move(deletedTransactions), Messages::DeleteTransaction::Reason::Outdated));
            }
        }
        catch (System::InterruptedException &)
        {
            logger(Logging::DEBUGGING) << "transactionPoolCleaningProcedure has been interrupted";
        }
        catch (std::exception &e)
        {
            logger(Logging::ERROR) << "Error occurred while cleaning transactions pool: " << e.what();
        }
    }

    void Core::updateBlockMedianSize()
    {
        auto mainChain = chainsLeaves[0];

        size_t nextBlockGrantedFullRewardZone = currency.blockGrantedFullRewardZoneByBlockVersion(
            upgradeManager->getBlockMajorVersion(mainChain->getTopBlockIndex() + 1));

        auto lastBlockSizes = mainChain->getLastBlocksSizes(currency.rewardBlocksWindow());

        blockMedianSize =
            std::max(Common::medianValue(lastBlockSizes), static_cast<uint64_t>(nextBlockGrantedFullRewardZone));
    }

    uint64_t Core::get_current_blockchain_height() const
    {
        // TODO: remove when GetCoreStatistics is implemented
        return mainChainStorage->getBlockCount();
    }

    std::time_t Core::getStartTime() const
    {
        return start_time;
    }

} // namespace CryptoNote
