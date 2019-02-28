// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <algorithm>

#include <memory>

#include <string>

#include <vector>

#include <iterator>

#include <zedwallet/Types.h>

void confirmPassword(const std::string &walletPass, const std::string &msg = "");

void removeCharFromString(std::string &str, const char c);

void trim(std::string &str);

void leftTrim(std::string &str);

void rightTrim(std::string &str);

bool confirm(const std::string &msg);
bool confirm(const std::string &msg, const bool defaultReturn);

bool startsWith(const std::string &str, const std::string &substring);

bool fileExists(const std::string &filename);

bool shutdown(std::shared_ptr<WalletInfo> walletInfo, CryptoNote::INode &node, bool &alreadyShuttingDown);

std::string formatAmountBasic(const uint64_t amount);
std::string formatAmount(const uint64_t amount);
std::string formatDollars(const uint64_t amount);
std::string formatCents(const uint64_t amount);

std::string getPaymentIDFromExtra(const std::string &extra);

std::string unixTimeToDate(const uint64_t timestamp);

std::string createIntegratedAddress(const std::string &address, const std::string &paymentID);

uint64_t getDivisor();

uint64_t getScanHeight();

template <typename T, typename Function> std::vector<T> filter(const std::vector<T> &input, Function predicate)
{
    std::vector<T> result;

    std::copy_if(input.begin(), input.end(), std::back_inserter(result), predicate);

    return result;
}

std::vector<std::string> split(const std::string &str, char delim);
bool parseDaemonAddressFromString(std::string &host, int &port, const std::string &address);