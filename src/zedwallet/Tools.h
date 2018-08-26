// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <zedwallet/Types.h>

void confirmPassword(std::string walletPass, std::string msg="");

bool confirm(std::string msg);
bool confirm(std::string msg, bool defaultReturn);

std::string formatAmountBasic(uint64_t amount);
std::string formatAmount(uint64_t amount);
std::string formatDollars(uint64_t amount);
std::string formatCents(uint64_t amount);

std::string getPaymentIDFromExtra(std::string extra);

std::string yellowANSIMsg(std::string msg);

std::string getPrompt(std::shared_ptr<WalletInfo> &walletInfo);

std::string unixTimeToDate(uint64_t timestamp);

std::string createIntegratedAddress(std::string address, std::string paymentID);

uint64_t getDivisor();

uint64_t getScanHeight();
