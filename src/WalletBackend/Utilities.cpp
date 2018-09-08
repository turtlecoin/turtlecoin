// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

////////////////////////////////////
#include <WalletBackend/Utilities.h>
////////////////////////////////////

#include <config/CryptoNoteConfig.h>

#include <CryptoNoteCore/CryptoNoteBasicImpl.h>

/* Will throw an exception if the addresses are invalid. Please check they
   are valid before calling this function. (e.g. use validateAddresses)
   
   Please note this function does not accept integrated addresses. Please
   extract the payment ID from them before calling this function. */
std::vector<Crypto::PublicKey> addressesToViewKeys(std::vector<std::string> addresses)
{
    std::vector<Crypto::PublicKey> viewKeys;

    CryptoNote::AccountPublicAddress parsedAddress;

    uint64_t prefix;

    for (const auto &address : addresses)
    {
        /* Failed to parse */
        if (!parseAccountAddressString(prefix, parsedAddress, address))
        {
            throw std::invalid_argument("Address is not valid!");
        }

        /* Incorrect prefix */
        if (prefix != CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX)
        {
            throw std::invalid_argument("Address is not valid!");
        }

        viewKeys.push_back(parsedAddress.viewPublicKey);
    }

    return viewKeys;
}
