// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

//////////////////////////////////////
#include <Errors/ValidateParameters.h>
//////////////////////////////////////

#include <Common/Base58.h>

#include <config/CryptoNoteConfig.h>
#include <config/WalletConfig.h>

extern "C"
{
    #include <crypto/crypto-ops.h>
}

#include <Common/TransactionExtra.h>
#include <Common/CryptoNoteTools.h>

#include <regex>

#include <Utilities/Addresses.h>
#include <Utilities/Mixins.h>
#include <Utilities/Utilities.h>

Error validateFusionTransaction(
    const uint64_t mixin,
    const std::vector<std::string> subWalletsToTakeFrom,
    const std::string destinationAddress,
    const std::shared_ptr<SubWallets> subWallets,
    const uint64_t currentHeight)
{
    /* Validate the mixin */
    if (Error error = validateMixin(mixin, currentHeight); error != SUCCESS)
    {
        return error;
    }
    
    /* Verify the subwallets to take from are valid and exist in the subwallets */
    if (Error error = validateOurAddresses({subWalletsToTakeFrom}, subWallets); error != SUCCESS)
    {
        return error;
    }

    /* Verify the destination address is valid and exists in the subwallets */
    if (Error error = validateOurAddresses({destinationAddress}, subWallets); error != SUCCESS)
    {
        return error;
    }

    return SUCCESS;
}

Error validateTransaction(
    const std::vector<std::pair<std::string, uint64_t>> destinations,
    const uint64_t mixin,
    const uint64_t fee,
    const std::string paymentID,
    const std::vector<std::string> subWalletsToTakeFrom,
    const std::string changeAddress,
    const std::shared_ptr<SubWallets> subWallets,
    const uint64_t currentHeight)
{
    /* Validate the destinations */
    if (Error error = validateDestinations(destinations); error != SUCCESS)
    {
        return error;
    }

    /* OK, destinations are good. Extract the regular addresses from the
       integrated addresses, and the payment ID's. Verify the paymentID's
       don't conflict */
    if (Error error = validateIntegratedAddresses(destinations, paymentID); error != SUCCESS)
    {
        return error;
    }

    /* Verify the subwallets to take from exist */
    if (Error error = validateOurAddresses(subWalletsToTakeFrom, subWallets); error != SUCCESS)
    {
        return error;
    }

    /* Validate we have enough money for the transaction */
    if (Error error = validateAmount(destinations, fee,
            subWalletsToTakeFrom, subWallets, currentHeight); error != SUCCESS)
    {
        return error;
    }

    /* Validate the mixin */
    if (Error error = validateMixin(mixin, currentHeight); error != SUCCESS)
    {
        return error;
    }

    /* Validate the payment ID */
    if (Error error = validatePaymentID(paymentID); error != SUCCESS)
    {
        return error;
    }

    /* Verify the change address is valid and exists in the subwallets */
    if (Error error = validateOurAddresses({changeAddress}, subWallets); error != SUCCESS)
    {
        return error;
    }

    return SUCCESS;
}

Error validateIntegratedAddresses(
    const std::vector<std::pair<std::string, uint64_t>> destinations,
    std::string paymentID)
{
    for (const auto [address, amount] : destinations)
    {
        if (address.length() != WalletConfig::integratedAddressLength)
        {
            continue;
        }

        /* Grab the address + pid from the integrated address */
        const auto [extractedAddress, extractedPaymentID] 
            = Utilities::extractIntegratedAddressData(address);

        /* No payment ID given, set it to the extracted one */
        if (paymentID == "")
        {
            paymentID = extractedPaymentID;
        }
        else if (paymentID != extractedPaymentID)
        {
            return CONFLICTING_PAYMENT_IDS;
        }
    }

    return SUCCESS;
}

Error validateHash(const std::string hash)
{
    if (hash.length() != 64)
    {
        return HASH_WRONG_LENGTH;
    }

    std::regex hexRegex("[a-zA-Z0-9]{64}");
    
    if (!std::regex_match(hash, hexRegex))
    {
        return HASH_INVALID;
    }

    return SUCCESS;
}

Error validatePaymentID(const std::string paymentID)
{
    if (paymentID.empty())
    {
        return SUCCESS;
    }

    if (paymentID.length() != 64)
    {
        return PAYMENT_ID_WRONG_LENGTH;
    }

    std::regex hexRegex("[a-zA-Z0-9]{64}");
    
    if (!std::regex_match(paymentID, hexRegex))
    {
        return PAYMENT_ID_INVALID;
    }

    return SUCCESS;
}

Error validatePrivateKey(const Crypto::SecretKey &privateViewKey)
{
    const bool valid = sc_check(reinterpret_cast<const unsigned char *>(&privateViewKey)) == 0;

    if (valid)
    {
        return SUCCESS;
    }
    else
    {
        return INVALID_PRIVATE_KEY;
    }
}

Error validatePublicKey(const Crypto::PublicKey &publicKey)
{
    const bool valid = Crypto::check_key(publicKey);

    if (valid)
    {
        return SUCCESS;
    }
    else
    {
        return INVALID_PUBLIC_KEY;
    }
}

Error validateMixin(const uint64_t mixin, const uint64_t height)
{
    const auto [minMixin, maxMixin, defaultMixin] = Utilities::getMixinAllowableRange(height);

    if (mixin < minMixin)
    {
        return Error(
            MIXIN_TOO_SMALL,
            "The mixin value given (" + std::to_string(mixin) + ") is lower "
            "than the minimum mixin allowed (" + std::to_string(minMixin) + ")"
        );
    }

    if (mixin > maxMixin)
    {
        return Error(
            MIXIN_TOO_BIG,
            "The mixin value given (" + std::to_string(mixin) + ") is greater "
            "than the maximum mixin allowed (" + std::to_string(maxMixin) + ")"
        );
    }

    return SUCCESS;
}

Error validateAmount(
    const std::vector<std::pair<std::string, uint64_t>> destinations,
    const uint64_t fee,
    const std::vector<std::string> subWalletsToTakeFrom,
    const std::shared_ptr<SubWallets> subWallets,
    const uint64_t currentHeight)
{
    /* Verify the fee is valid */
    if (fee < CryptoNote::parameters::MINIMUM_FEE)
    {
        return FEE_TOO_SMALL;
    }

    /* Get the available balance, using the source addresses */
    const auto [availableBalance, lockedBalance] = subWallets->getBalance(
        Utilities::addressesToSpendKeys(subWalletsToTakeFrom),
        /* Take from all if no subwallets specified */
        subWalletsToTakeFrom.empty(),
        currentHeight
    );

    /* Get the total amount of the transaction */
    uint64_t totalAmount = Utilities::getTransactionSum(destinations) + fee;

    std::vector<uint64_t> amounts { fee };

    std::transform(destinations.begin(), destinations.end(), std::back_inserter(amounts),
    [](const auto destination)
    {
        return destination.second;
    });

    /* Check the total amount we're sending is not >= uint64_t */
    if (Utilities::sumWillOverflow(amounts))
    {
        return WILL_OVERFLOW;
    }

    if (totalAmount > availableBalance)
    {
        return NOT_ENOUGH_BALANCE;
    }

    return SUCCESS;
}

Error validateDestinations(
    const std::vector<std::pair<std::string, uint64_t>> destinations)
{
    /* Make sure there is at least one destination */
    if (destinations.empty())
    {
        return NO_DESTINATIONS_GIVEN;
    }

    std::vector<std::string> destinationAddresses;

    for (const auto & [destination, amount] : destinations)
    {
        /* Check all of the amounts are > 0 */
        if (amount == 0)
        {
            return AMOUNT_IS_ZERO;
        }

        destinationAddresses.push_back(destination);
    }

    /* Validate the addresses are good [Integrated addresses allowed] */
    if (Error error = validateAddresses(destinationAddresses, true); error != SUCCESS)
    {
        return error;
    }
    
    return SUCCESS;
}

Error validateAddresses(
    std::vector<std::string> addresses,
    const bool integratedAddressesAllowed)
{
    for (auto &address : addresses)
    {
        /* Address is the wrong length */
        if (address.length() != WalletConfig::standardAddressLength &&
            address.length() != WalletConfig::integratedAddressLength)
        {
            std::stringstream stream;

            stream << "The address given is the wrong length. It should be "
                   << WalletConfig::standardAddressLength << " chars or "
                   << WalletConfig::integratedAddressLength << " chars, but "
                   << "it is " << address.length() << " chars.";

            return Error(ADDRESS_WRONG_LENGTH, stream.str());
        }

        /* Address has the wrong prefix */
        if (address.substr(0, WalletConfig::addressPrefix.length()) !=
            WalletConfig::addressPrefix)
        {
            return ADDRESS_WRONG_PREFIX;
        }

        if (address.length() == WalletConfig::integratedAddressLength)
        {
            if (!integratedAddressesAllowed)
            {
                return Error(
                    ADDRESS_IS_INTEGRATED,
                    "The address given (" + address + ") is an integrated "
                    "address, but integrated addresses aren't valid for "
                    "this parameter."
                );
            }

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
            address = Utilities::getAccountAddressAsStr(
                CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
                addr
            );
        }

        /* Not used */
        uint64_t ignore;

        /* Not used */
        CryptoNote::AccountPublicAddress ignore2;

        if (!Utilities::parseAccountAddressString(ignore, ignore2, address))
        {
            return ADDRESS_NOT_VALID;
        }
    }

    return SUCCESS;
}

Error validateOurAddresses(
    const std::vector<std::string> addresses,
    const std::shared_ptr<SubWallets> subWallets)
{
    /* Validate the addresses are valid [Integrated addreses not allowed] */
    if (Error error = validateAddresses(addresses, false); error != SUCCESS)
    {
        return error;
    }

    for (const auto address : addresses)
    {
        const auto [spendKey, viewKey] = Utilities::addressToKeys(address);

        const auto &keys = subWallets->m_publicSpendKeys;

        if (std::find(keys.begin(), keys.end(), spendKey) == keys.end())
        {
            return Error(
                ADDRESS_NOT_IN_WALLET,
                "The address given (" + address + ") does not exist in the "
                "wallet container, but it is required to exist for this operation."
            );
        }
    }

    return SUCCESS;
}
