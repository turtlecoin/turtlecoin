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

namespace
{

    uint64_t getCurrentTimestampAdjusted()
    {
        /* Get the current time as a unix timestamp */
        std::time_t time = std::time(nullptr);

        /* Take the amount of time a block can potentially be in the past/future */
        std::initializer_list<uint64_t> limits =
        {
            CryptoNote::parameters::CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT,
            CryptoNote::parameters::CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V3,
            CryptoNote::parameters::CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V4
        };

        /* Get the largest adjustment possible */
        uint64_t adjust = std::max(limits);

        /* Take the earliest timestamp that will include all possible blocks */
        return time - adjust;
    }

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

        /* Get the genesis block timestamp and add the time since launch */
        uint64_t timestamp = CryptoNote::parameters::GENESIS_BLOCK_TIMESTAMP
                           + secondsSinceLaunch;

        /* Don't make timestamp too large or daemon throws an error */
        if (timestamp >= getCurrentTimestampAdjusted())
        {
            return getCurrentTimestampAdjusted();
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

/* Makes a view only subwallet */
SubWallet::SubWallet(const Crypto::PublicKey publicSpendKey,
                     const std::string address,
                     const uint64_t scanHeight, const bool newWallet) :
    m_publicSpendKey(publicSpendKey),
    m_address(address),
    /* If we're making a new wallet, sync from now, else sync from the scan
       height (normally creation height) */
    m_syncStartTimestamp(newWallet ? getCurrentTimestampAdjusted() 
                                   : scanHeightToTimestamp(scanHeight)),
    m_isViewWallet(true)
{
}

/* Makes a standard subwallet */
SubWallet::SubWallet(const Crypto::PublicKey publicSpendKey,
                     const Crypto::SecretKey privateSpendKey,
                     const std::string address,
                     const uint64_t scanHeight, const bool newWallet) :

    m_publicSpendKey(publicSpendKey),
    m_address(address),
    /* If we're making a new wallet, sync from now, else sync from the scan
       height (normally creation height) */
    m_syncStartTimestamp(newWallet ? getCurrentTimestampAdjusted() 
                                   : scanHeightToTimestamp(scanHeight)),
    m_isViewWallet(false),
    m_privateSpendKey(privateSpendKey)
{
}

/////////////////////
/* CLASS FUNCTIONS */
/////////////////////

void SubWallet::generateAndStoreKeyImage(Crypto::KeyDerivation derivation,
                                         size_t outputIndex)
{
    if (m_isViewWallet)
    {
        throw std::invalid_argument(
            "Attempted to generate key image from view only subwallet!"
        );
    }

    Crypto::KeyImage keyImage;

    /* Make a temporary key pair */
    CryptoNote::KeyPair tmp;

    /* Get the tmp public key from the derivation, the index,
       and our public spend key */
    Crypto::derive_public_key(
        derivation, outputIndex, m_publicSpendKey, tmp.publicKey
    );

    /* Get the tmp private key from the derivation, the index,
       and our private spend key */
    Crypto::derive_secret_key(
        derivation, outputIndex, m_privateSpendKey, tmp.secretKey
    );

    /* Get the key image from the tmp public and private key */
    Crypto::generate_key_image(
        tmp.publicKey, tmp.secretKey, keyImage
    );

    m_keyImages.insert(keyImage);
}
