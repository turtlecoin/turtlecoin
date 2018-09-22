// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

////////////////////////////////////
#include <WalletBackend/SubWallet.h>
////////////////////////////////////

#include <config/CryptoNoteConfig.h>

#include <CryptoNoteCore/Account.h>
#include <CryptoNoteCore/CryptoNoteBasicImpl.h>

#include "json.hpp"

using json = nlohmann::json;

#include <WalletBackend/JsonSerialization.h>

//////////////////////////
/* NON MEMBER FUNCTIONS */
//////////////////////////

namespace {

    /* Converts a height to a timestamp, conservatively */
    uint64_t scanHeightToTimestamp(const uint64_t scanHeight)
    {
        if (scanHeight == 0)
        {
            return 0;
        }

        /* Get the amount of seconds since the blockchain launched */
        uint64_t secondsSinceLaunch = scanHeight * 
                                      CryptoNote::parameters::DIFFICULTY_TARGET;

        /* Add a bit of a buffer in case of difficulty weirdness, blocks coming
           out too fast */
        secondsSinceLaunch *= 0.95;

        /* Get the genesis block timestamp and add the time since launch */
        uint64_t timestamp = CryptoNote::parameters::GENESIS_BLOCK_TIMESTAMP
                           + secondsSinceLaunch;

        /* Timestamp in the future */
        if (timestamp >= static_cast<uint64_t>(std::time(nullptr)))
        {
            return std::time(nullptr);
        }

        return timestamp;
    }

} // namespace

///////////////////////////////////
/* CONSTRUCTORS / DECONSTRUCTORS */
///////////////////////////////////

SubWallet::SubWallet()
{
}

SubWallet::SubWallet(const Crypto::SecretKey privateSpendKey,
                     const std::string address,
                     const uint64_t scanHeight, const bool newWallet) :
    m_privateSpendKey(privateSpendKey),
    m_address(address),
    /* If we're making a new wallet, sync from now, else sync from the scan
       height (normally creation height) */
    m_syncStartTimestamp(newWallet ? std::time(nullptr) : scanHeightToTimestamp(scanHeight))
{
}

/////////////////////
/* CLASS FUNCTIONS */
/////////////////////
