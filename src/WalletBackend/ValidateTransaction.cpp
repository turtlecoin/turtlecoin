// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

//////////////////////////////////////////////
#include <WalletBackend/ValidateTransaction.h>
//////////////////////////////////////////////

#include <Common/Base58.h>

#include <config/CryptoNoteConfig.h>
#include <config/WalletConfig.h>

#include <CryptoNoteCore/CryptoNoteBasicImpl.h>
#include <CryptoNoteCore/CryptoNoteTools.h>
#include <CryptoNoteCore/TransactionExtra.h>

WalletError validateTransaction(
    std::unordered_map<std::string, uint64_t> destinations,
    uint64_t mixin, uint64_t fee, std::string paymentID,
    std::vector<std::string> subWalletsToTakeFrom, std::string changeAddress)
{
    std::vector<std::string> addressesToValidate { changeAddress };

    for (const auto &destination : destinations)
    {
        addressesToValidate.push_back(destination.first);
    }

    /* Validate the destination and change address */
    WalletError error = validateAddresses(addressesToValidate);

    if (error)
    {
        return error;
    }

    return SUCCESS;
}

/* Returns an error code if the address is invalid */
WalletError validateAddresses(std::vector<std::string> addresses)
{
    for (auto &address : addresses)
    {
        /* Address is the wrong length */
        if (address.length() != WalletConfig::standardAddressLength &&
            address.length() != WalletConfig::integratedAddressLength)
        {
            return ADDRESS_WRONG_LENGTH;
        }

        /* Address has the wrong prefix */
        if (address.substr(0, WalletConfig::addressPrefix.length()) !=
            WalletConfig::addressPrefix)
        {
            return ADDRESS_WRONG_PREFIX;
        }

        if (address.length() == WalletConfig::integratedAddressLength)
        {
            std::string decoded;

            /* Don't need this */
            uint64_t ignore;

            if (!Tools::Base58::decode_addr(address, ignore, decoded))
            {
                return ADDRESS_NOT_BASE58;
            }

            const uint64_t paymentIDLen = 64;

            /* Grab the payment ID from the decoded address */
            std::string paymentID = decoded.substr(0, paymentIDLen);

            std::vector<uint8_t> extra;

            /* Verify the extracted payment ID is valid */
            if (!CryptoNote::createTxExtraWithPaymentId(paymentID, extra))
            {
                return INTEGRATED_ADDRESS_PAYMENT_ID_INVALID;
            }

            /* The binary array encoded keys are the rest of the address */
            std::string keys = decoded.substr(paymentIDLen, std::string::npos);

            /* Convert keys as string to binary array */
            CryptoNote::BinaryArray ba = Common::asBinaryArray(keys);

            CryptoNote::AccountPublicAddress addr;

            /* Convert from binary array to public keys */
            if (!CryptoNote::fromBinaryArray(addr, ba))
            {
                return ADDRESS_NOT_VALID;
            }

            /* Convert the set of extracted keys back into an address, then
               verify that as a normal address */
            address = CryptoNote::getAccountAddressAsStr(
                CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
                addr
            );
        }

        /* Not used */
        uint64_t ignore;

        /* Not used */
        CryptoNote::AccountPublicAddress ignore2;

        if (!parseAccountAddressString(ignore, ignore2, address))
        {
            return ADDRESS_NOT_VALID;
        }
    }

    return SUCCESS;
}
