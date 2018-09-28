// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

/////////////////////////////////////
#include <WalletBackend/SubWallets.h>
/////////////////////////////////////

/* TODO: Remove */
#include <Common/StringTools.h>

#include <crypto/crypto.h>

/* TODO: Remove */
#include <iostream>

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

void SubWallets::addTransaction(Transaction tx)
{
    m_transactions.push_back(tx);

    /* We can regenerate the balance from the transactions, but this will be
       faster, as getting the balance is a common operation */
    for (const auto &transfer : tx.transfers)
    {
        auto pubKey = transfer.first;
        auto amount = std::abs(transfer.second);

        m_subWallets[pubKey].balance += amount;

        
        if (amount != 0 && tx.fee == 0)
        {
            std::cout << "Coinbase transaction found!" << std::endl;
        }
        else if (amount > 0)
        {
            std::cout << "Incoming transaction found!" << std::endl;
        }
        else if (amount < 0)
        {
            std::cout << "Outgoing transaction found!" << std::endl;
        }
        else
        {
            std::cout << "Fusion transaction found!" << std::endl;
        }

        std::cout << "Hash: " << Common::podToHex(tx.hash) << std::endl
                  << "Amount: " << amount << std::endl
                  << "Fee: " << tx.fee << std::endl
                  << "Block height: " << tx.blockHeight << std::endl
                  << "Timestamp: " << tx.timestamp << std::endl << std::endl;
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
