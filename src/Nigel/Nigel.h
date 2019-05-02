// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <atomic>

#include "httplib.h"

#include <Rpc/CoreRpcServerCommandsDefinitions.h>

#include <config/CryptoNoteConfig.h>

#include <string>

#include <thread>

#include <unordered_set>

#include "WalletTypes.h"

class Nigel
{
    public:

        /////////////////////////
        /* Public Constructors */
        /////////////////////////

        Nigel(
            const std::string daemonHost,
            const uint16_t daemonPort,
            const bool daemonSSL);

        Nigel(
            const std::string daemonHost,
            const uint16_t daemonPort,
            const bool daemonSSL,
            const std::chrono::seconds timeout);

        ~Nigel();

        /////////////////////////////
        /* Public member functions */
        /////////////////////////////

        void init();

        void swapNode(const std::string daemonHost, const uint16_t daemonPort, const bool daemonSSL);

        void decreaseRequestedBlockCount();

        void resetRequestedBlockCount();

        /* Returns whether we've received info from the daemon at some point */
        bool isOnline() const;

        uint64_t localDaemonBlockCount() const;

        uint64_t networkBlockCount() const;

        uint64_t peerCount() const;

        uint64_t hashrate() const;

        std::tuple<uint64_t, std::string> nodeFee() const;

        std::tuple<std::string, uint16_t, bool> nodeAddress() const;

        std::tuple<bool, std::vector<WalletTypes::WalletBlockInfo>> getWalletSyncData(
            const std::vector<Crypto::Hash> blockHashCheckpoints,
            uint64_t startHeight,
            uint64_t startTimestamp) const;

        /* Returns a bool on success or not */
        bool getTransactionsStatus(
            const std::unordered_set<Crypto::Hash> transactionHashes,
            std::unordered_set<Crypto::Hash> &transactionsInPool,
            std::unordered_set<Crypto::Hash> &transactionsInBlock,
            std::unordered_set<Crypto::Hash> &transactionsUnknown) const;

        std::tuple<bool, std::vector<CryptoNote::RandomOuts>> getRandomOutsByAmounts(
            const std::vector<uint64_t> amounts,
            const uint64_t requestedOuts) const;

        /* {success, connectionError} */
        std::tuple<bool, bool> sendTransaction(
            const CryptoNote::Transaction tx) const;

        std::tuple<bool, std::unordered_map<Crypto::Hash, std::vector<uint64_t>>>
            getGlobalIndexesForRange(
                const uint64_t startHeight,
                const uint64_t endHeight) const;

    private:

        //////////////////////////////
        /* Private member functions */
        //////////////////////////////

        void stop();

        void backgroundRefresh();

        bool getDaemonInfo();

        bool getFeeInfo();

        //////////////////////////////
        /* Private member variables */
        //////////////////////////////

        /* Stores our http client (Don't really care about it launching threads
           and making our functions non const) */
        std::shared_ptr<httplib::Client> m_nodeClient = nullptr;

        /* Runs a background refresh on height, hashrate, etc */
        std::thread m_backgroundThread;

        /* If we should stop the background thread */
        std::atomic<bool> m_shouldStop = false;

        /* Stores how many blocks we'll try to sync */
        std::atomic<uint64_t> m_blockCount = CryptoNote::BLOCKS_SYNCHRONIZING_DEFAULT_COUNT;

        /* The amount of blocks the daemon we're connected to has */
        std::atomic<uint64_t> m_localDaemonBlockCount = 0;

        /* The amount of blocks the network has */
        std::atomic<uint64_t> m_networkBlockCount = 0;

        /* The amount of peers we're connected to */
        std::atomic<uint64_t> m_peerCount = 0;

        /* The hashrate (based on the last local block the daemon has synced) */
        std::atomic<uint64_t> m_lastKnownHashrate = 0;

        /* Whether the daemon is a blockchain cache API
           see: https://github.com/TurtlePay/blockchain-cache-api */
        std::atomic<bool> m_isBlockchainCache = false;

        /* The address to send the node fee to (May be "") */
        std::string m_nodeFeeAddress;

        /* The fee the node charges */
        uint64_t m_nodeFeeAmount = 0;

        /* The timeout on requests */
        std::chrono::seconds m_timeout;

        /* The daemon hostname */
        std::string m_daemonHost;

        /* The daemon port */
        uint16_t m_daemonPort;

        /* If the daemon is SSL */
        bool m_daemonSSL = false;
};
