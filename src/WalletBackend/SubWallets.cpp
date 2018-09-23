// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

/////////////////////////////////////
#include <WalletBackend/SubWallets.h>
/////////////////////////////////////

#include <crypto/crypto.h>

//////////////////////////
/* NON MEMBER FUNCTIONS */
//////////////////////////

namespace {
} // namespace

///////////////////////////////////
/* CONSTRUCTORS / DECONSTRUCTORS */
///////////////////////////////////

SubWallets::SubWallets()
{
}

SubWallets::SubWallets(const Crypto::PublicKey publicSpendKey,
                       const std::string address,
                       const uint64_t scanHeight, const bool newWallet)
{
    m_subWallets[publicSpendKey]
        = SubWallet(publicSpendKey, address, scanHeight, newWallet);

    m_publicSpendKeys.push_back(publicSpendKey);
}

SubWallets::SubWallets(const Crypto::SecretKey privateSpendKey,
                       const std::string address,
                       const uint64_t scanHeight, const bool newWallet)
{
    Crypto::PublicKey publicSpendKey;

    Crypto::secret_key_to_public_key(privateSpendKey, publicSpendKey);

    m_subWallets[publicSpendKey]
        = SubWallet(publicSpendKey, address, scanHeight, newWallet);

    m_publicSpendKeys.push_back(publicSpendKey);
}

/////////////////////
/* CLASS FUNCTIONS */
/////////////////////

void SubWallets::addSubWallet(const Crypto::PublicKey publicSpendKey,
                              const std::string address,
                              const uint64_t scanHeight, const bool newWallet)
{
    m_subWallets[publicSpendKey]
        = SubWallet(publicSpendKey, address, scanHeight, newWallet);

    m_publicSpendKeys.push_back(publicSpendKey);
}

/* Gets the smallest timestamp of all the sub wallets */
uint64_t SubWallets::getMinSyncTimestamp()
{
    /* Get the smallest sub wallet (by timestamp) */
    auto min = *std::min_element(m_subWallets.begin(), m_subWallets.end(),
    [](const std::pair<Crypto::PublicKey, SubWallet> &lhs,
       const std::pair<Crypto::PublicKey, SubWallet> &rhs)
    {
        return lhs.second.m_syncStartTimestamp < rhs.second.m_syncStartTimestamp;
    });

    /* Return the smallest wallets timestamp */
    return min.second.m_syncStartTimestamp;
}

void SubWallets::addTransfers(std::unordered_map<Crypto::PublicKey, uint64_t>
                              transfers)
{
    for (const auto &x : transfers)
    {
        m_subWallets[x.first].addTransfer(x.second);
    }
}
