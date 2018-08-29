// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <algorithm>

#include <string>

#include <vector>

#include <iterator>

void confirmPassword(std::string walletPass, std::string msg="");

bool confirm(std::string msg);
bool confirm(std::string msg, bool defaultReturn);

std::string formatAmountBasic(uint64_t amount);
std::string formatAmount(uint64_t amount);
std::string formatDollars(uint64_t amount);
std::string formatCents(uint64_t amount);

std::string getPaymentIDFromExtra(std::string extra);

std::string unixTimeToDate(uint64_t timestamp);

std::string createIntegratedAddress(std::string address, std::string paymentID);

uint64_t getDivisor();

uint64_t getScanHeight();

template <typename T, typename Function>
std::vector<T> filter(std::vector<T> input, Function predicate)
{
    std::vector<T> result;

    std::copy_if(
        input.begin(), input.end(), std::back_inserter(result), predicate
    );

    return result;
}
