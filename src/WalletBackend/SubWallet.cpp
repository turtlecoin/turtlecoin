// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

////////////////////////////////////////
#include <WalletBackend/SubWallet.h>
////////////////////////////////////////

#include "CryptoNoteConfig.h"

#include <CryptoNoteCore/Account.h>
#include <CryptoNoteCore/CryptoNoteBasicImpl.h>

#include "json.hpp"

using json = nlohmann::json;

#include <WalletBackend/JsonSerialization.h>

//////////////////////////
/* NON MEMBER FUNCTIONS */
//////////////////////////

namespace {
} // namespace

/////////////////////
/* CLASS FUNCTIONS */
/////////////////////

SubWallet::SubWallet(Crypto::SecretKey privateSpendKey, std::string address,
                     uint64_t scanHeight, bool newWallet) :
    m_privateSpendKey(privateSpendKey),
    m_address(address),
    m_scanHeight(scanHeight)
{
    if (newWallet)
    {
        m_creationTimestamp = std::time(nullptr);
    }
}
