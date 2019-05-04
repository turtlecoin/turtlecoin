// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

//////////////////////////////////
#include <SubWallets/SubWallets.h>
//////////////////////////////////

#include <config/CryptoNoteConfig.h>

#include <ctime>

#include <mutex>

#include <random>

#include <Utilities/Addresses.h>
#include <Utilities/Utilities.h>

///////////////////////////////////
/* CONSTRUCTORS / DECONSTRUCTORS */
///////////////////////////////////

/* Makes a new subwallet */
SubWallets::SubWallets(
    const Crypto::SecretKey privateSpendKey,
    const Crypto::SecretKey privateViewKey,
    const std::string address,
    const uint64_t scanHeight,
    const bool newWallet) :

    m_privateViewKey(privateViewKey),
    m_isViewWallet(false)
{
    Crypto::PublicKey publicSpendKey;

    Crypto::secret_key_to_public_key(privateSpendKey, publicSpendKey);

    const uint64_t timestamp = newWallet ? Utilities::getCurrentTimestampAdjusted() : 0;

    const bool isPrimaryAddress = true;

    m_subWallets[publicSpendKey] = SubWallet(
        publicSpendKey, privateSpendKey, address, scanHeight, timestamp,
        isPrimaryAddress
    );

    m_publicSpendKeys.push_back(publicSpendKey);
}

/* Makes a new view only subwallet */
SubWallets::SubWallets(
    const Crypto::SecretKey privateViewKey,
    const std::string address,
    const uint64_t scanHeight,
    const bool newWallet) :

    m_privateViewKey(privateViewKey),
    m_isViewWallet(true)
{
    const auto [publicSpendKey, publicViewKey] = Utilities::addressToKeys(address);

    const uint64_t timestamp = newWallet ? Utilities::getCurrentTimestampAdjusted() : 0;

    const bool isPrimaryAddress = true;

    m_subWallets[publicSpendKey] = SubWallet(
        publicSpendKey, address, scanHeight, timestamp, isPrimaryAddress
    );

    m_publicSpendKeys.push_back(publicSpendKey);
}

/* Copy constructor */
SubWallets::SubWallets(const SubWallets &other) :
    m_subWallets(other.m_subWallets),
    m_transactions(other.m_transactions),
    m_lockedTransactions(other.m_lockedTransactions),
    m_privateViewKey(other.m_privateViewKey),
    m_isViewWallet(other.m_isViewWallet),
    m_publicSpendKeys(other.m_publicSpendKeys),
    m_transactionPrivateKeys(other.m_transactionPrivateKeys)
{
}

/////////////////////
/* CLASS FUNCTIONS */
/////////////////////

std::tuple<Error, std::string, Crypto::SecretKey> SubWallets::addSubWallet()
{
    /* This generates a private spend key - incompatible with view wallets */
    if (m_isViewWallet)
    {
        return {ILLEGAL_VIEW_WALLET_OPERATION, std::string(), Crypto::SecretKey()};
    }

    std::scoped_lock lock(m_mutex);

    CryptoNote::KeyPair spendKey;

    /* Generate a spend key */
    Crypto::generate_keys(spendKey.publicKey, spendKey.secretKey);

    const std::string address = Utilities::privateKeysToAddress(
        spendKey.secretKey, m_privateViewKey
    );

    const bool isPrimaryAddress = false;

    const uint64_t scanHeight = 0;

    m_subWallets[spendKey.publicKey] = SubWallet(
        spendKey.publicKey, spendKey.secretKey, address, scanHeight,
        Utilities::getCurrentTimestampAdjusted(), isPrimaryAddress
    );

    m_publicSpendKeys.push_back(spendKey.publicKey);

    return {SUCCESS, address, spendKey.secretKey};
}

std::tuple<Error, std::string> SubWallets::importSubWallet(
    const Crypto::SecretKey privateSpendKey,
    const uint64_t scanHeight)
{
    /* Can't add a private spend key to a view wallet */
    if (m_isViewWallet)
    {
        return {ILLEGAL_VIEW_WALLET_OPERATION, std::string()};
    }

    std::scoped_lock lock(m_mutex);

    Crypto::PublicKey publicSpendKey;

    Crypto::secret_key_to_public_key(privateSpendKey, publicSpendKey);

    uint64_t timestamp = 0;

    const std::string address = Utilities::privateKeysToAddress(
        privateSpendKey, m_privateViewKey
    );

    const bool isPrimaryAddress = false;

    if (m_subWallets.find(publicSpendKey) != m_subWallets.end())
    {
        return {SUBWALLET_ALREADY_EXISTS, std::string()};
    }

    m_subWallets[publicSpendKey] = SubWallet(
        publicSpendKey, privateSpendKey, address, scanHeight, timestamp,
        isPrimaryAddress
    );

    m_publicSpendKeys.push_back(publicSpendKey);

    return {SUCCESS, address};
}

std::tuple<Error, std::string> SubWallets::importViewSubWallet(
    const Crypto::PublicKey publicSpendKey,
    const uint64_t scanHeight)
{
    /* Can't have view/non view wallets in one container */
    if (!m_isViewWallet)
    {
        return {ILLEGAL_NON_VIEW_WALLET_OPERATION, std::string()};
    }

    std::scoped_lock lock(m_mutex);

    if (m_subWallets.find(publicSpendKey) != m_subWallets.end())
    {
        return {SUBWALLET_ALREADY_EXISTS, std::string()};
    }

    uint64_t timestamp = 0;

    Crypto::PublicKey publicViewKey;

    Crypto::secret_key_to_public_key(m_privateViewKey, publicViewKey);

    const std::string address = Utilities::publicKeysToAddress(
        publicSpendKey, publicViewKey
    );

    const bool isPrimaryAddress = false;

    m_subWallets[publicSpendKey] = SubWallet(
        publicSpendKey, address, scanHeight, timestamp, isPrimaryAddress
    );

    m_publicSpendKeys.push_back(publicSpendKey);

    return {SUCCESS, address};
}

Error SubWallets::deleteSubWallet(const std::string address)
{
    std::scoped_lock lock(m_mutex);

    const auto [spendKey, viewKey] = Utilities::addressToKeys(address);

    const auto it = m_subWallets.find(spendKey);

    if (it == m_subWallets.end())
    {
        return ADDRESS_NOT_IN_WALLET;
    }

    /* We can't delete the primary address */
    if (it->second.isPrimaryAddress())
    {
        return CANNOT_DELETE_PRIMARY_ADDRESS;
    }

    m_subWallets.erase(it);

    /* Remove or update the transactions */
    deleteAddressTransactions(m_transactions, spendKey);
    deleteAddressTransactions(m_lockedTransactions, spendKey);

    const auto it2 = std::remove(m_publicSpendKeys.begin(), m_publicSpendKeys.end(), spendKey);

    if (it2 != m_publicSpendKeys.end())
    {
        m_publicSpendKeys.erase(it2, m_publicSpendKeys.end());
    }

    return SUCCESS;
}

void SubWallets::deleteAddressTransactions(
    std::vector<WalletTypes::Transaction> &txs,
    const Crypto::PublicKey spendKey)
{
    const auto it = std::remove_if(txs.begin(), txs.end(), [spendKey](auto &tx)
    {
        /* See if this transaction contains the subwallet we're deleting */
        const auto key = tx.transfers.find(spendKey);

        /* OK, it does */
        if (key != tx.transfers.end())
        {
            /* It's the only element, delete the transaction */
            if (tx.transfers.size() == 1)
            {
                return true;
            }
            /* Otherwise just delete the transfer in the transaction */
            else
            {
                tx.transfers.erase(key);
            }
        }

        return false;
    });

    if (it != txs.end())
    {
        txs.erase(it, txs.end());
    }
}

/* Gets the starting height, and timestamp to begin the sync from. Only one of
   these will be non zero, which will the the lowest one (ignoring null values).

   So, if for example, one subwallet has a start height of 400,000, and another
   has a timestamp of something corresponding to 300,000, we would return
   zero for the start height, and the timestamp corresponding to 300,000.

   Alternatively, if the timestamp corresponded to 500,000, we would return
   400,000 for the height, and zero for the timestamp. */
std::tuple<uint64_t, uint64_t> SubWallets::getMinInitialSyncStart() const
{
    std::scoped_lock lock(m_mutex);

    /* Get the smallest sub wallet (by timestamp) */
    auto minElementByTimestamp = *std::min_element(m_subWallets.begin(), m_subWallets.end(),
    [](const auto &lhs, const auto &rhs)
    {
        return lhs.second.syncStartTimestamp() < rhs.second.syncStartTimestamp();
    });

    const uint64_t minTimestamp = minElementByTimestamp.second.syncStartTimestamp();

    /* Get the smallest sub wallet (by height) */
    auto minElementByHeight = *std::min_element(m_subWallets.begin(), m_subWallets.end(),
    [](const auto &lhs, const auto &rhs)
    {
        return lhs.second.syncStartHeight() < rhs.second.syncStartHeight();
    });

    const uint64_t minHeight = minElementByHeight.second.syncStartHeight();

    /* One or both of the values are zero, caller will use whichever is non
       zero */
    if (minHeight == 0 || minTimestamp == 0)
    {
        return {minHeight, minTimestamp};
    }

    /* Convert timestamp to height so we can compare them, then return the min
       of the two, and set the other to zero */
    const uint64_t timestampFromHeight = Utilities::scanHeightToTimestamp(minHeight);

    if (timestampFromHeight < minTimestamp)
    {
        return {minHeight, 0};
    }
    else
    {
        return {0, minTimestamp};
    }
}

void SubWallets::addUnconfirmedTransaction(const WalletTypes::Transaction tx)
{
    std::scoped_lock lock(m_mutex);

    const auto it2 = std::find_if(m_lockedTransactions.begin(), m_lockedTransactions.end(),
    [tx](const auto transaction)
    {
        return tx.hash == transaction.hash;
    });

    if (it2 != m_lockedTransactions.end())
    {
        std::stringstream stream;

        stream << "Unconfirmed transaction " << tx.hash << " was added to the wallet twice! Terminating.";

        throw std::runtime_error(stream.str());
    }

    m_lockedTransactions.push_back(tx);
}

void SubWallets::addTransaction(const WalletTypes::Transaction tx)
{
    std::scoped_lock lock(m_mutex);

    /* If we sent this transaction, we will input it into the transactions
       vector instantly. This lets us display the data to the user, and then
       when the transaction actually comes in, we will update the transaction
       with the block infomation. */
    const auto it = std::remove_if(m_lockedTransactions.begin(), m_lockedTransactions.end(),
    [tx](const auto transaction)
    {
        return tx.hash == transaction.hash;
    });

    if (it != m_lockedTransactions.end())
    {
        /* Remove from the locked container */
        m_lockedTransactions.erase(it, m_lockedTransactions.end());
    }

    const auto it2 = std::find_if(m_transactions.begin(), m_transactions.end(),
    [tx](const auto transaction)
    {
        return tx.hash == transaction.hash;
    });

    if (it2 != m_transactions.end())
    {
        std::stringstream stream;

        stream << "Transaction " << tx.hash << " was added to the wallet twice! Terminating.";

        throw std::runtime_error(stream.str());
    }

    m_transactions.push_back(tx);
}

Crypto::KeyImage SubWallets::getTxInputKeyImage(
    const Crypto::PublicKey publicSpendKey,
    const Crypto::KeyDerivation derivation,
    const size_t outputIndex) const
{
    std::scoped_lock lock(m_mutex);

    const auto it = m_subWallets.find(publicSpendKey);

    /* Check it exists */
    if (it != m_subWallets.end())
    {
        /* If we have a view wallet, don't attempt to derive the key image */
        return it->second.getTxInputKeyImage(
            derivation, outputIndex, m_isViewWallet
        );
    }

    throw std::runtime_error("Subwallet not found!");
}

void SubWallets::storeTransactionInput(
    const Crypto::PublicKey publicSpendKey,
    const WalletTypes::TransactionInput input)
{
    std::scoped_lock lock(m_mutex);

    const auto it = m_subWallets.find(publicSpendKey);

    /* Check it exists */
    if (it != m_subWallets.end())
    {
        if (!m_isViewWallet)
        {
            /* Add the new key image to the store, so we can detect when we
               spent a key image easily */
            m_keyImageOwners[input.keyImage] = publicSpendKey;
        }

        /* If we have a view wallet, don't attempt to derive the key image */
        return it->second.storeTransactionInput(input, m_isViewWallet);
    }

    throw std::runtime_error("Subwallet not found!");
}

std::tuple<bool, Crypto::PublicKey>
    SubWallets::getKeyImageOwner(const Crypto::KeyImage keyImage) const
{
    const auto it = m_keyImageOwners.find(keyImage);

    if (it != m_keyImageOwners.end())
    {
        return {true, it->second};
    }

    return {false, Crypto::PublicKey()};
}

/* Remember if the transaction suceeds, we need to remove these key images
   so we don't double spend.
   
   This may throw if you don't validate the user has enough balance, and
   that each of the subwallets exist. */
std::tuple<std::vector<WalletTypes::TxInputAndOwner>, uint64_t>
        SubWallets::getTransactionInputsForAmount(
    const uint64_t amount,
    const bool takeFromAll,
    std::vector<Crypto::PublicKey> subWalletsToTakeFrom,
    const uint64_t height) const
{
    /* Can't send transactions with a view wallet */
    throwIfViewWallet();

    std::scoped_lock lock(m_mutex);

    /* If we're able to take from every subwallet, set the wallets to take from
       to all our public spend keys */
    if (takeFromAll)
    {
        subWalletsToTakeFrom = m_publicSpendKeys;
    }

    std::vector<SubWallet> wallets;

    /* Loop through each public key and grab the associated wallet */
    for (const auto &publicKey : subWalletsToTakeFrom)
    {
        wallets.push_back(m_subWallets.at(publicKey));
    }

    std::vector<WalletTypes::TxInputAndOwner> availableInputs;

    /* Copy the transaction inputs from this sub wallet to inputs */
    for (const auto &subWallet : wallets)
    {
        const auto moreInputs = subWallet.getSpendableInputs(height);

        availableInputs.insert(availableInputs.end(), moreInputs.begin(), moreInputs.end());
    }

    /* Shuffle the inputs */
    std::shuffle(availableInputs.begin(), availableInputs.end(), std::random_device{});

    uint64_t foundMoney = 0;

    std::vector<WalletTypes::TxInputAndOwner> inputsToUse;

    /* Loop through each input */
    for (const auto walletAmount : availableInputs)
    {
        /* Add each input */
        inputsToUse.push_back(walletAmount);

        foundMoney += walletAmount.input.amount;

        /* Keep adding until we have enough money for the transaction */
        if (foundMoney >= amount)
        {
            return {inputsToUse, foundMoney};
        }
    }

    /* Not enough money to cover the transaction */
    throw std::invalid_argument("Not enough funds found!");
}

/* Remember if the transaction suceeds, we need to remove these key images
   so we don't double spend. */
std::tuple<std::vector<WalletTypes::TxInputAndOwner>, uint64_t, uint64_t>
    SubWallets::getFusionTransactionInputs(
    const bool takeFromAll,
    std::vector<Crypto::PublicKey> subWalletsToTakeFrom,
    const uint64_t mixin,
    const uint64_t height) const
{
    /* Can't send transactions with a view wallet */
    throwIfViewWallet();

    std::scoped_lock lock(m_mutex);

    /* If we're able to take from every subwallet, set the wallets to take from
       to all our public spend keys */
    if (takeFromAll)
    {
        subWalletsToTakeFrom = m_publicSpendKeys;
    }

    std::vector<SubWallet> wallets;

    /* Loop through each public key and grab the associated wallet */
    for (const auto &publicKey : subWalletsToTakeFrom)
    {
        wallets.push_back(m_subWallets.at(publicKey));
    }

    std::vector<WalletTypes::TxInputAndOwner> availableInputs;

    /* Copy the transaction inputs from this sub wallet to inputs */
    for (const auto &subWallet : wallets)
    {
        const auto moreInputs = subWallet.getSpendableInputs(height);

        availableInputs.insert(availableInputs.end(), moreInputs.begin(), moreInputs.end());
    }

    /* Get an approximation of the max amount of inputs we can include in this
       transaction */
    uint64_t maxInputsToTake = Utilities::getApproximateMaximumInputCount(
        CryptoNote::parameters::FUSION_TX_MAX_SIZE,
        CryptoNote::parameters::FUSION_TX_MIN_IN_OUT_COUNT_RATIO,
        mixin
    );

    /* Shuffle the inputs */
    std::shuffle(availableInputs.begin(), availableInputs.end(), std::random_device{});

    /* Split the inputs into buckets based on what power of ten they are in
       (For example, [1, 2, 5, 7], [20, 50, 80, 80], [100, 600, 700]) */
    std::unordered_map<uint64_t, std::vector<WalletTypes::TxInputAndOwner>> buckets;

    for (const auto &walletAmount : availableInputs)
    {
        /* Find out how many digits the amount has, i.e. 1337 has 4 digits,
           420 has 3 digits */
        int numberOfDigits = floor(log10(walletAmount.input.amount)) + 1;

        /* Insert the amount into the correct bucket */
        buckets[numberOfDigits].push_back(walletAmount);
    }

    /* Find the buckets which have enough inputs to meet the fusion tx
       requirements */
    std::vector<std::vector<WalletTypes::TxInputAndOwner>> fullBuckets;

    for (const auto [amount, bucket] : buckets)
    {
        /* Skip the buckets with not enough items */
        if (bucket.size() >= CryptoNote::parameters::FUSION_TX_MIN_INPUT_COUNT)
        {
            fullBuckets.push_back(bucket);
        }
    }

    /* Shuffle the full buckets */
    std::shuffle(fullBuckets.begin(), fullBuckets.end(), std::random_device{});

    /* The buckets to pick inputs from */
    std::vector<std::vector<WalletTypes::TxInputAndOwner>> bucketsToTakeFrom;

    /* We have full buckets, take the first full bucket */
    if (!fullBuckets.empty())
    {
        bucketsToTakeFrom = { fullBuckets.front() };
    }
    /* Otherwise just use all buckets */
    else
    {
        for (const auto [amount, bucket] : buckets)
        {
            bucketsToTakeFrom.push_back(bucket);
        }
    }

    std::vector<WalletTypes::TxInputAndOwner> inputsToUse;

    uint64_t foundMoney = 0;

    /* Loop through each bucket (Remember we're only looping through one if
       we've got a full bucket) */
    for (const auto bucket : bucketsToTakeFrom)
    {
        /* Loop through each input in this bucket */
        for (const auto &walletAmount : bucket)
        {
            /* Add each input */
            inputsToUse.push_back(walletAmount);

            foundMoney += walletAmount.input.amount;

            /* Got enough inputs, return */
            if (inputsToUse.size() >= maxInputsToTake)
            {
                return {inputsToUse, maxInputsToTake, foundMoney};
            }
        }
    }

    return {inputsToUse, maxInputsToTake, foundMoney};
}

/* Gets the primary address, which is the first address created with the
   wallet */
std::string SubWallets::getPrimaryAddress() const
{
    std::scoped_lock lock(m_mutex);

    const auto it = 
    std::find_if(m_subWallets.begin(), m_subWallets.end(), [](const auto subWallet)
    {
        return subWallet.second.isPrimaryAddress();
    });

    if (it == m_subWallets.end())
    {
        throw std::runtime_error("This container has no primary address!");
    }

    return it->second.address();
}

std::vector<std::string> SubWallets::getAddresses() const
{
    std::vector<std::string> addresses;

    for (const auto &[pubKey, subWallet] : m_subWallets)
    {
        addresses.push_back(subWallet.address());
    }

    return addresses;
}

uint64_t SubWallets::getWalletCount() const
{
    return m_subWallets.size();
}

/* Will throw if the public keys given don't exist */
std::tuple<uint64_t, uint64_t> SubWallets::getBalance(
    std::vector<Crypto::PublicKey> subWalletsToTakeFrom,
    const bool takeFromAll,
    const uint64_t currentHeight) const
{
    std::scoped_lock lock(m_mutex);

    /* If we're able to take from every subwallet, set the wallets to take from
       to all our public spend keys */
    if (takeFromAll)
    {
        subWalletsToTakeFrom = m_publicSpendKeys;
    }

    uint64_t unlockedBalance = 0;

    uint64_t lockedBalance = 0;

    for (const auto &pubKey : subWalletsToTakeFrom)
    {
        const auto [unlocked, locked] = m_subWallets.at(pubKey).getBalance(currentHeight);

        unlockedBalance += unlocked;
        lockedBalance += locked;
    }

    return {unlockedBalance, lockedBalance};
}

/* Mark a key image as spent, no longer can be used in transactions */
void SubWallets::markInputAsSpent(
    const Crypto::KeyImage keyImage,
    const Crypto::PublicKey publicKey,
    const uint64_t spendHeight)
{
    /* A view wallet can't generate key images, so can't determine when an
       input is spent */
    throwIfViewWallet();

    std::scoped_lock lock(m_mutex);

    m_subWallets.at(publicKey).markInputAsSpent(keyImage, spendHeight);
}

/* Mark a key image as locked, can no longer be used in transactions till it
   returns from the pool, or we find it in a block, in which case we will
   mark it as spent. */
void SubWallets::markInputAsLocked(
    const Crypto::KeyImage keyImage,
    const Crypto::PublicKey publicKey)
{
    /* View wallets can't have locked inputs (can't spend) */
    throwIfViewWallet();

    std::scoped_lock lock(m_mutex);

    m_subWallets.at(publicKey).markInputAsLocked(keyImage);
}

/* Remove transactions and key images that occured on a forked chain */
void SubWallets::removeForkedTransactions(uint64_t forkHeight)
{
    std::scoped_lock lock(m_mutex);

    const auto it = std::remove_if(m_transactions.begin(), m_transactions.end(),
    [forkHeight](auto tx)
    {
        /* Remove the transaction if it's height is >= than the fork height */
        return tx.blockHeight >= forkHeight;
    });

    if (it != m_transactions.end())
    {
        m_transactions.erase(it, m_transactions.end());
    }

    /* Loop through each subwallet */
    for (auto & [publicKey, subWallet] : m_subWallets)
    {
        subWallet.removeForkedInputs(forkHeight);
    }
}

void SubWallets::removeCancelledTransactions(
    const std::unordered_set<Crypto::Hash> cancelledTransactions)
{
    /* View wallets don't have locked transactions (can't spend) */
    throwIfViewWallet();

    std::scoped_lock lock(m_mutex);

    /* Find any cancelled transactions */
    const auto it = std::remove_if(m_lockedTransactions.begin(), m_lockedTransactions.end(),
    [&cancelledTransactions](const auto &tx)
    {
        return cancelledTransactions.find(tx.hash) != cancelledTransactions.end();
    });

    if (it != m_lockedTransactions.end())
    {
        /* Remove the cancelled transactions */
        m_lockedTransactions.erase(it, m_lockedTransactions.end());
    }

    for (auto &[pubKey, subWallet] : m_subWallets)
    {
        subWallet.removeCancelledTransactions(cancelledTransactions);
    }
}

Crypto::SecretKey SubWallets::getPrivateViewKey() const
{
    return m_privateViewKey;
}

std::tuple<Error, Crypto::SecretKey> SubWallets::getPrivateSpendKey(
    const Crypto::PublicKey publicSpendKey) const
{
    throwIfViewWallet();

    const auto it = m_subWallets.find(publicSpendKey);

    if (it == m_subWallets.end())
    {
        return {ADDRESS_NOT_IN_WALLET, Crypto::SecretKey()};
    }

    return {SUCCESS, it->second.privateSpendKey()};
}

std::unordered_set<Crypto::Hash> SubWallets::getLockedTransactionsHashes() const
{
    /* Can't have locked transactions in a view wallet (can't spend) */
    throwIfViewWallet();

    std::scoped_lock lock(m_mutex);

    std::unordered_set<Crypto::Hash> result;

    for (const auto transaction : m_lockedTransactions)
    {
        result.insert(transaction.hash);
    }

    return result;
}

void SubWallets::throwIfViewWallet() const
{
    if (m_isViewWallet)
    {
        throw std::runtime_error("Wallet is a view wallet, but this function cannot be called in a view wallet");
    }
}

bool SubWallets::isViewWallet() const
{
    return m_isViewWallet;
}

void SubWallets::reset(const uint64_t scanHeight)
{
    std::scoped_lock lock(m_mutex);

    m_lockedTransactions.clear();
    m_transactions.clear();
    m_transactionPrivateKeys.clear();

    for (auto &[pubKey, subWallet] : m_subWallets)
    {
        subWallet.reset(scanHeight);
    }
}

std::vector<Crypto::SecretKey> SubWallets::getPrivateSpendKeys() const
{
    std::vector<Crypto::SecretKey> spendKeys;

    for (const auto &[pubKey, subWallet] : m_subWallets)
    {
        spendKeys.push_back(subWallet.privateSpendKey());
    }

    return spendKeys;
}

Crypto::SecretKey SubWallets::getPrimaryPrivateSpendKey() const
{
    std::scoped_lock lock(m_mutex);

    const auto it = 
    std::find_if(m_subWallets.begin(), m_subWallets.end(), [](const auto subWallet)
    {
        return subWallet.second.isPrimaryAddress();
    });

    if (it == m_subWallets.end())
    {
        throw std::runtime_error("This container has no primary address!");
    }

    return it->second.privateSpendKey();
}

std::vector<WalletTypes::Transaction> SubWallets::getTransactions() const
{
    return m_transactions;
}

/* Note that this DOES NOT return incoming transactions in the pool. It only
   returns outgoing transactions which we sent but have not encountered in a
   block yet. */
std::vector<WalletTypes::Transaction> SubWallets::getUnconfirmedTransactions() const
{
    return m_lockedTransactions;
}

std::tuple<Error, std::string> SubWallets::getAddress(
    const Crypto::PublicKey spendKey) const
{
    const auto it = m_subWallets.find(spendKey);

    if (it != m_subWallets.end())
    {
        return {SUCCESS, it->second.address()};
    }

    return {ADDRESS_NOT_IN_WALLET, std::string()};
}

void SubWallets::storeTxPrivateKey(
    const Crypto::SecretKey txPrivateKey,
    const Crypto::Hash txHash)
{
    m_transactionPrivateKeys[txHash] = txPrivateKey;
}

std::tuple<bool, Crypto::SecretKey> SubWallets::getTxPrivateKey(
    const Crypto::Hash txHash) const
{
    const auto it = m_transactionPrivateKeys.find(txHash);

    if (it != m_transactionPrivateKeys.end())
    {
        return {true, it->second};
    }

    return {false, Crypto::SecretKey()};
}

void SubWallets::storeUnconfirmedIncomingInput(
    const WalletTypes::UnconfirmedInput input,
    const Crypto::PublicKey publicSpendKey)
{
    std::scoped_lock lock(m_mutex);

    const auto it = m_subWallets.find(publicSpendKey);

    if (it != m_subWallets.end())
    {
        it->second.storeUnconfirmedIncomingInput(input);
    }
}

void SubWallets::convertSyncTimestampToHeight(
    const uint64_t timestamp,
    const uint64_t height)
{
    std::scoped_lock lock(m_mutex);

    for (auto &[pubKey, subWallet] : m_subWallets)
    {
        subWallet.convertSyncTimestampToHeight(timestamp, height);
    }
}

std::vector<std::tuple<std::string, uint64_t, uint64_t>> SubWallets::getBalances(
    const uint64_t currentHeight) const
{
    std::vector<std::tuple<std::string, uint64_t, uint64_t>> balances;

    for (const auto &[pubKey, subWallet] : m_subWallets)
    {
        const auto [unlocked, locked] = subWallet.getBalance(currentHeight);

        balances.emplace_back(subWallet.address(), unlocked, locked);
    }

    return balances;
}

void SubWallets::pruneSpentInputs(const uint64_t pruneHeight)
{
    for (auto &[pubKey, subWallet] : m_subWallets)
    {
        subWallet.pruneSpentInputs(pruneHeight);
    }
}

void SubWallets::fromJSON(const JSONObject &j)
{
    for (const auto &x : getArrayFromJSON(j, "publicSpendKeys"))
    {
        Crypto::PublicKey key;
        key.fromString(getStringFromJSONString(x));
        m_publicSpendKeys.push_back(key);
    }

    for (const auto &x : getArrayFromJSON(j, "subWallet"))
    {
        SubWallet s;
        s.fromJSON(x);
        m_subWallets[s.publicSpendKey()] = s;

        /* Load the key images hashmap from the loaded subwallets */
        if (!m_isViewWallet)
        {
            for (const auto &[pubKey, subWallet] : m_subWallets)
            {
                for (const auto &keyImage : subWallet.getKeyImages())
                {
                    m_keyImageOwners[keyImage] = pubKey;
                }
            }
        }
    }

    for (const auto &x : getArrayFromJSON(j, "transactions"))
    {
        WalletTypes::Transaction tx;
        tx.fromJSON(x);
        m_transactions.push_back(tx);
    }

    for (const auto &x : getArrayFromJSON(j, "lockedTransactions"))
    {
        WalletTypes::Transaction tx;
        tx.fromJSON(x);
        m_lockedTransactions.push_back(tx);
    }

    m_privateViewKey.fromString(getStringFromJSON(j, "privateViewKey"));

    m_isViewWallet = getBoolFromJSON(j, "isViewWallet");

    for (const auto &txKey : getArrayFromJSON(j, "txPrivateKeys"))
    {
        Crypto::Hash txHash;
        txHash.fromString(getStringFromJSON(txKey, "transactionHash"));

        Crypto::SecretKey privateKey;
        privateKey.fromString(getStringFromJSON(txKey, "txPrivateKey"));

        m_transactionPrivateKeys[txHash] = privateKey;
    }
}

void SubWallets::toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
{
    writer.StartObject();

    writer.Key("publicSpendKeys");
    writer.StartArray();
    for (const auto &key : m_publicSpendKeys)
    {
        key.toJSON(writer);
    }
    writer.EndArray();

    writer.Key("subWallet");
    writer.StartArray();
    for (const auto &[publicKey, subWallet] : m_subWallets)
    {
        subWallet.toJSON(writer);
    }
    writer.EndArray();

    writer.Key("transactions");
    writer.StartArray();
    for (const auto &tx : m_transactions)
    {
        tx.toJSON(writer);
    }
    writer.EndArray();

    writer.Key("lockedTransactions");
    writer.StartArray();
    for (const auto &tx : m_lockedTransactions)
    {
        tx.toJSON(writer);
    }
    writer.EndArray();

    writer.Key("privateViewKey");
    m_privateViewKey.toJSON(writer);

    writer.Key("isViewWallet");
    writer.Bool(m_isViewWallet);

    writer.Key("txPrivateKeys");
    writer.StartArray();
    for (const auto [txHash, txPrivateKey] : m_transactionPrivateKeys)
    {
        writer.StartObject();

        writer.Key("transactionHash");
        txHash.toJSON(writer);

        writer.Key("txPrivateKey");
        txPrivateKey.toJSON(writer);

        writer.EndObject();
    }
    writer.EndArray();

    writer.EndObject();
}

