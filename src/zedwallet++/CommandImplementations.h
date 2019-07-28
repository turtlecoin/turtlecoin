// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <walletbackend/WalletBackend.h>

void changePassword(const std::shared_ptr<WalletBackend> walletBackend);

void printPrivateKeys(const std::shared_ptr<WalletBackend> walletBackend);

void reset(const std::shared_ptr<WalletBackend> walletBackend);

void status(const std::shared_ptr<WalletBackend> walletBackend);

void printHeights(
    const uint64_t localDaemonBlockCount,
    const uint64_t networkBlockCount,
    const uint64_t walletBlockCount);

void printSyncStatus(
    const uint64_t localDaemonBlockCount,
    const uint64_t networkBlockCount,
    const uint64_t walletBlockCount);

void printSyncSummary(
    const uint64_t localDaemonBlockCount,
    const uint64_t networkBlockCount,
    const uint64_t walletBlockCount);

void printHashrate(const uint64_t difficulty);

void balance(const std::shared_ptr<WalletBackend> walletBackend);

void backup(const std::shared_ptr<WalletBackend> walletBackend);

void saveCSV(const std::shared_ptr<WalletBackend> walletBackend);

void save(const std::shared_ptr<WalletBackend> walletBackend);

void listTransfers(const bool incoming, const bool outgoing, const std::shared_ptr<WalletBackend> walletBackend);

void printOutgoingTransfer(const WalletTypes::Transaction t);

void printIncomingTransfer(const WalletTypes::Transaction t);

void createIntegratedAddress();

void help(const std::shared_ptr<WalletBackend> walletBackend);

void advanced(const std::shared_ptr<WalletBackend> walletBackend);

void swapNode(const std::shared_ptr<WalletBackend> walletBackend);

void getTxPrivateKey(const std::shared_ptr<WalletBackend> walletBackend);

void setLogLevel();
