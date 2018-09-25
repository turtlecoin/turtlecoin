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

/* Makes a new view only subwallet */
SubWallets::SubWallets(const Crypto::PublicKey publicSpendKey,
                       const std::string address,
                       const uint64_t scanHeight, const bool newWallet)
{
    m_subWallets[publicSpendKey]
        = SubWallet(publicSpendKey, address, scanHeight, newWallet);

    m_publicSpendKeys.push_back(publicSpendKey);
}

/* Makes a new subwallet */
SubWallets::SubWallets(const Crypto::SecretKey privateSpendKey,
                       const std::string address,
                       const uint64_t scanHeight, const bool newWallet)
{
    Crypto::PublicKey publicSpendKey;

    Crypto::secret_key_to_public_key(privateSpendKey, publicSpendKey);

    m_subWallets[publicSpendKey] = SubWallet(
        publicSpendKey, privateSpendKey, address, scanHeight, newWallet
    );

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

void SubWallets::addSubWallet(const Crypto::SecretKey privateSpendKey,
                              const std::string address,
                              const uint64_t scanHeight, const bool newWallet)
{
    Crypto::PublicKey publicSpendKey;

    Crypto::secret_key_to_public_key(privateSpendKey, publicSpendKey);

    m_subWallets[publicSpendKey] = SubWallet(
        publicSpendKey, privateSpendKey, address, scanHeight, newWallet
    );

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

void SubWallets::addTransfers(std::unordered_map<Crypto::PublicKey, int64_t>
                              transfers)
{
    for (const auto &x : transfers)
    {
        m_subWallets[x.first].addTransfer(x.second);
    }
}

void SubWallets::generateAndStoreKeyImage(Crypto::PublicKey publicSpendKey,
                                          Crypto::KeyDerivation derivation,
                                          size_t outputIndex)
{
    const auto subWallet = m_subWallets.find(publicSpendKey);

    /* Check it exists, and it isn't a view wallet */
    if (subWallet != m_subWallets.end() && !subWallet->second.m_isViewWallet)
    {
        subWallet->second.generateAndStoreKeyImage(derivation, outputIndex);
    }
}

std::tuple<bool, Crypto::PublicKey>
    SubWallets::getKeyImageOwner(Crypto::KeyImage keyImage)
{
    for (const auto &x : m_subWallets)
    {
        const SubWallet subWallet = x.second;

        if (subWallet.m_keyImages.find(keyImage) != subWallet.m_keyImages.end())
        {
            return std::make_tuple(true, subWallet.m_publicSpendKey);
        }
    }

    return std::make_tuple(false, Crypto::PublicKey());
}
