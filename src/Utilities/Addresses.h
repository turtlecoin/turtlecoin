// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoNote.h>

#include <Errors/Errors.h>

#include <string>

#include <vector>

namespace Utilities
{
    std::vector<Crypto::PublicKey> addressesToSpendKeys(const std::vector<std::string> addresses);

    std::tuple<Crypto::PublicKey, Crypto::PublicKey> addressToKeys(const std::string address);

    std::tuple<std::string, std::string> extractIntegratedAddressData(const std::string address);

    std::string publicKeysToAddress(
        const Crypto::PublicKey publicSpendKey,
        const Crypto::PublicKey publicViewKey);

    std::string privateKeysToAddress(
        const Crypto::SecretKey privateSpendKey,
        const Crypto::SecretKey privateViewKey);

    std::tuple<Error, std::string> createIntegratedAddress(
        const std::string address,
        const std::string paymentID);

    std::string getAccountAddressAsStr(
        const uint64_t prefix,
        const CryptoNote::AccountPublicAddress& adr);

    bool parseAccountAddressString(
        uint64_t& prefix,
        CryptoNote::AccountPublicAddress& adr,
        const std::string& str);
}
