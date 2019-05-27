// Copyright (c) 2019, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

//////////////////////////////////////////
#include <WalletBackend/BlockDownloader.h>
//////////////////////////////////////////

#include <config/WalletConfig.h>

#include <Logger/Logger.h>

#include <Utilities/FormatTools.h>

#include <WalletBackend/Constants.h>

/* Constructor */
BlockDownloader::BlockDownloader(
    const std::shared_ptr<Nigel> daemon,
    const std::shared_ptr<SubWallets> subWallets,
    const uint64_t startHeight,
    const uint64_t startTimestamp) :

    m_daemon(daemon),
    m_subWallets(subWallets),
    m_startHeight(startHeight),
    m_startTimestamp(startTimestamp)
{
}

/* Move constructor */
BlockDownloader::BlockDownloader(BlockDownloader && old)
{
    /* Call the move assignment operator */
    *this = std::move(old);
}

/* Move assignment operator */
BlockDownloader& BlockDownloader::operator=(BlockDownloader && old)
{
    /* Stop any running threads */
    stop();

    m_storedBlocks = std::move(old.m_storedBlocks);

    m_daemon = std::move(old.m_daemon);

    m_startTimestamp = std::move(old.m_startTimestamp);
    m_startHeight = std::move(old.m_startHeight);

    m_synchronizationStatus = std::move(old.m_synchronizationStatus);

    m_consumedData = std::move(old.m_consumedData.load());

    m_shouldStop = std::move(old.m_shouldStop.load());

    return *this;
}

/* Destructor */
BlockDownloader::~BlockDownloader()
{
    stop();
}

void BlockDownloader::start()
{
    m_shouldStop = false;
    m_downloadThread = std::thread(&BlockDownloader::downloader, this);
}

void BlockDownloader::stop()
{
    m_shouldStop = true;
    m_consumedData = true;
    m_shouldTryFetch.notify_one();

    if (m_downloadThread.joinable())
    {
        m_downloadThread.join();
    }
}

uint64_t BlockDownloader::getHeight() const
{
    return m_synchronizationStatus.getHeight();
}

void BlockDownloader::downloader()
{
    while (!m_shouldStop)
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_shouldTryFetch.wait(lock, [&]
            {
                if (m_shouldStop)
                {
                    return true;
                }

                return m_consumedData.load();
            });
        }

        if (m_shouldStop)
        {
            break;
        }

        while (shouldFetchMoreBlocks() && !m_shouldStop)
        {
            if (!downloadBlocks())
            {
                break;
            }
        }

        m_consumedData = false;
    }
}

bool BlockDownloader::shouldFetchMoreBlocks() const
{
    size_t ramUsage = m_storedBlocks.memoryUsage([](const auto block) {
        return std::get<0>(block).memoryUsage();
    });

    if (ramUsage + WalletConfig::maxBodyResponseSize < WalletConfig::blockStoreMemoryLimit)
    {
        std::stringstream stream;

        stream << "Approximate ram usage of stored blocks: " << Utilities::prettyPrintBytes(ramUsage)
               << ", fetching more.";

        Logger::logger.log(
            stream.str(),
            Logger::DEBUG,
            {Logger::SYNC}
        );

        return true;
    }

    return false;
}

void BlockDownloader::dropBlock(const uint64_t blockHeight, const Crypto::Hash blockHash)
{
    m_storedBlocks.pop_front();
    m_synchronizationStatus.storeBlockHash(blockHash, blockHeight);

    /* Indicate to the downloader that it should try and download more */
    std::lock_guard<std::mutex> lock(m_mutex);
    m_consumedData = true;
    m_shouldTryFetch.notify_one();
}

std::vector<std::tuple<WalletTypes::WalletBlockInfo, uint32_t>> BlockDownloader::fetchBlocks(const size_t blockCount)
{
    /* Attempt to fetch more blocks if we've run out */
    if (m_storedBlocks.size() == 0)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_consumedData = true;
        m_shouldTryFetch.notify_one();

        return {};
    }

    const auto blocks = m_storedBlocks.front_n(blockCount);

    Logger::logger.log(
        "Fetched " + std::to_string(blocks.size()) + " blocks from internal store",
        Logger::DEBUG,
        {Logger::SYNC}
    );

    return blocks;
}

std::vector<Crypto::Hash> BlockDownloader::getStoredBlockCheckpoints() const
{
    const auto blocks = m_storedBlocks.back_n(Constants::LAST_KNOWN_BLOCK_HASHES_SIZE);

    std::vector<Crypto::Hash> result;

    result.resize(blocks.size());

    std::transform(blocks.begin(), blocks.end(), result.begin(), [](const auto block)
    {
        return std::get<0>(block).blockHash;
    });

    return result;
}

std::vector<Crypto::Hash> BlockDownloader::getBlockCheckpoints() const
{
    /* Hashes of blocks we have downloaded but not processed */
    const auto unprocessedBlockHashes = getStoredBlockCheckpoints();

    std::vector<Crypto::Hash> result(unprocessedBlockHashes.size());

    std::copy(unprocessedBlockHashes.begin(), unprocessedBlockHashes.end(), result.begin());

    /* Hashes of blocks we have processed in the wallet */
    const auto recentProcessedBlockHashes = m_synchronizationStatus.getRecentBlockHashes();

    /* If we don't have the desired 50 blocks, add on the recently processed
       block checkpoints. This fixes us not passing the right data when
       we are fully synced or have no store built up yet */
    if (result.size() < Constants::LAST_KNOWN_BLOCK_HASHES_SIZE)
    {
        /* Copy the amount of hashes available, or the amount needed to make
           up the difference, whichever is less */
        const size_t numToCopy = std::min(
            recentProcessedBlockHashes.size(),
            Constants::LAST_KNOWN_BLOCK_HASHES_SIZE - result.size()
        );

        std::copy(recentProcessedBlockHashes.begin(), recentProcessedBlockHashes.begin() + numToCopy, std::back_inserter(result));
    }

    /* Infrequent checkpoints to handle deep forks */
    const auto blockHashCheckpoints = m_synchronizationStatus.getBlockCheckpoints();

    std::copy(blockHashCheckpoints.begin(), blockHashCheckpoints.end(), std::back_inserter(result));

    return result;
}

bool BlockDownloader::downloadBlocks()
{
    const uint64_t localDaemonBlockCount = m_daemon->localDaemonBlockCount();

    const uint64_t walletBlockCount = m_synchronizationStatus.getHeight();

    if (localDaemonBlockCount < walletBlockCount)
    {
        return false;
    }

    const auto blockCheckpoints = getBlockCheckpoints();

    if (blockCheckpoints.size() > 0)
    {
        std::stringstream stream;

        stream << "First checkpoint: " << blockCheckpoints.front()
                  << "\nLast checkpoint: " << blockCheckpoints.back();

        Logger::logger.log(
            stream.str(),
            Logger::DEBUG,
            {Logger::SYNC}
        );
    }

    const auto [success, blocks] = m_daemon->getWalletSyncData(
        blockCheckpoints, m_startHeight, m_startTimestamp
    );

    /* If we get no blocks, we are fully synced.
       (Or timed out/failed to get blocks)
       Sleep a bit so we don't spam the daemon. */
    if (!success || blocks.empty())
    {
        /* We may have also failed because we requested
           more data than could be returned in a reasonable
           amount of time, so we'll back off a little bit */
        m_daemon->decreaseRequestedBlockCount();

        Logger::logger.log(
            "Zero blocks received from daemon, possibly fully synced",
            Logger::DEBUG,
            {Logger::SYNC}
        );

        return false;
    }

    /* If we received data back, we'll make sure we're back
       to running at full speed in case we backed off a little
       bit before */
    m_daemon->resetRequestedBlockCount();

    /* Timestamp is transient and can change - block height is constant. */
    if (m_startTimestamp != 0)
    {
        m_startTimestamp = 0;
        m_startHeight = blocks.front().blockHeight;

        m_subWallets->convertSyncTimestampToHeight(m_startTimestamp, m_startHeight);
    }

    /* If checkpoints are empty, this is the first sync request. */
    if (blockCheckpoints.empty())
    {
        /* Only check if a timestamp isn't given */
        if (m_startTimestamp == 0)
        {
            /* Loop through the blocks we got back and make sure that
               we were given data for the start block we were looking for */
            const auto it = std::find_if(blocks.begin(), blocks.end(), [this](const auto &block) {
                return block.blockHeight == m_startHeight;
            });

            /* If we weren't given a block with the startHeight we were
               looking for then we don't need to store this data */
            if (it == blocks.end())
            {
                std::stringstream stream;

                stream << "Received unexpected block height from daemon. "
                       << "Expected " << m_startHeight << ", but did not "
                       << "receive that block. Not storing any blocks.";

                Logger::logger.log(
                    stream.str(),
                    Logger::WARNING,
                    {Logger::SYNC, Logger::DAEMON}
                );

                return false;
            }
        }
    }

    std::stringstream stream;

    stream << "Downloaded " << blocks.size() << " blocks from daemon, ["
           << blocks.front().blockHeight << ", " << blocks.back().blockHeight
           << "]";

    Logger::logger.log(
        stream.str(),
        Logger::DEBUG,
        {Logger::SYNC}
    );

    std::vector<std::tuple<WalletTypes::WalletBlockInfo, uint32_t>> blocksWithIndex;

    for (const auto block : blocks)
    {
        blocksWithIndex.push_back({block, m_arrivalIndex++});
    }

    m_storedBlocks.push_back_n(blocksWithIndex.begin(), blocksWithIndex.end());

    return true;
}

void BlockDownloader::fromJSON(
    const JSONObject &j,
    const uint64_t startHeight,
    const uint64_t startTimestamp)
{
    m_synchronizationStatus.fromJSON(j);
    m_startHeight = startHeight;
    m_startTimestamp = startTimestamp;
}

void BlockDownloader::toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
{
    m_synchronizationStatus.toJSON(writer);
}

void BlockDownloader::setSubWallets(const std::shared_ptr<SubWallets> subWallets)
{
    m_subWallets = subWallets;
}

void BlockDownloader::initializeAfterLoad(const std::shared_ptr<Nigel> daemon)
{
    m_daemon = daemon;
}
