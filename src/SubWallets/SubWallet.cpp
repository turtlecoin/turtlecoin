// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

/////////////////////////////////
#include <SubWallets/SubWallet.h>
/////////////////////////////////

#include <config/Constants.h>

#include <Logger/Logger.h>

#include <Utilities/Utilities.h>

#include <WalletBackend/Constants.h>

///////////////////////////////////
/* CONSTRUCTORS / DECONSTRUCTORS */
///////////////////////////////////

/* Makes a view only subwallet */
SubWallet::SubWallet(
    const Crypto::PublicKey publicSpendKey,
    const std::string address,
    const uint64_t scanHeight,
    const uint64_t scanTimestamp,
    const bool isPrimaryAddress) :

    m_publicSpendKey(publicSpendKey),
    m_address(address),
    m_syncStartHeight(scanHeight),
    m_syncStartTimestamp(scanTimestamp),
    m_privateSpendKey(Constants::NULL_SECRET_KEY),
    m_isPrimaryAddress(isPrimaryAddress)
{
}

/* Makes a standard subwallet */
SubWallet::SubWallet(
    const Crypto::PublicKey publicSpendKey,
    const Crypto::SecretKey privateSpendKey,
    const std::string address,
    const uint64_t scanHeight,
    const uint64_t scanTimestamp,
    const bool isPrimaryAddress) :

    m_publicSpendKey(publicSpendKey),
    m_address(address),
    m_syncStartHeight(scanHeight),
    m_syncStartTimestamp(scanTimestamp),
    m_privateSpendKey(privateSpendKey),
    m_isPrimaryAddress(isPrimaryAddress)
{
}

/////////////////////
/* CLASS FUNCTIONS */
/////////////////////

Crypto::KeyImage SubWallet::getTxInputKeyImage(
    const Crypto::KeyDerivation derivation,
    const size_t outputIndex,
    const bool isViewWallet) const
{
    /* Can't create a key image with a view wallet - but we still store the
       input so we can calculate the balance */
    if (!isViewWallet)
    {
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

        return keyImage;
    }

    return Crypto::KeyImage();
}

void SubWallet::storeTransactionInput(
    const WalletTypes::TransactionInput input,
    const bool isViewWallet)
{
    /* Can't create a key image with a view wallet - but we still store the
       input so we can calculate the balance */
    if (!isViewWallet)
    {
        /* Find the input in the unconfirmed incoming amounts - inputs we
           sent ourselves, that are now returning as change. Remove from
           vector if found. */
        const auto it = std::remove_if(m_unconfirmedIncomingAmounts.begin(), m_unconfirmedIncomingAmounts.end(),
        [&input](const auto storedInput)
        {
            return storedInput.key == input.key;
        });

        if (it != m_unconfirmedIncomingAmounts.end())
        {
            m_unconfirmedIncomingAmounts.erase(it, m_unconfirmedIncomingAmounts.end());
        }
    }

    m_unspentInputs.push_back(input);
}

std::tuple<uint64_t, uint64_t> SubWallet::getBalance(
    const uint64_t currentHeight) const
{
    uint64_t unlockedBalance = 0;

    uint64_t lockedBalance = 0;

    for (const auto input : m_unspentInputs)
    {
        /* If an unlock height is present, check if the input is unlocked */
        if (Utilities::isInputUnlocked(input.unlockTime, currentHeight))
        {
            unlockedBalance += input.amount;
        }
        else
        {
            lockedBalance += input.amount;
        }
    }

    /* Add the locked balance from incoming transactions */
    for (const auto unconfirmedInput : m_unconfirmedIncomingAmounts)
    {
        lockedBalance += unconfirmedInput.amount;
    }

    return {unlockedBalance, lockedBalance};
}

void SubWallet::reset(const uint64_t scanHeight)
{
    m_syncStartTimestamp = 0;
    m_syncStartHeight = scanHeight;

    m_lockedInputs.clear();
    m_unconfirmedIncomingAmounts.clear();
    m_unspentInputs.clear();
    m_spentInputs.clear();
}

bool SubWallet::isPrimaryAddress() const
{
    return m_isPrimaryAddress;
}

std::string SubWallet::address() const
{
    return m_address;
}

Crypto::PublicKey SubWallet::publicSpendKey() const
{
    return m_publicSpendKey;
}

Crypto::SecretKey SubWallet::privateSpendKey() const
{
    return m_privateSpendKey;
}

void SubWallet::markInputAsSpent(
    const Crypto::KeyImage keyImage,
    const uint64_t spendHeight)
{
    /* Find the input */
    auto it = std::find_if(m_unspentInputs.begin(), m_unspentInputs.end(),
    [&keyImage](const auto x)
    {
        return x.keyImage == keyImage;
    });

    if (it != m_unspentInputs.end())
    {
        /* Set the spend height */
        it->spendHeight = spendHeight;

        /* Add to the spent inputs vector */
        m_spentInputs.push_back(*it);

        /* Remove from the unspent vector */
        m_unspentInputs.erase(it);

        return;
    }

    /* Didn't find it, lets try in the locked inputs */
    it = std::find_if(m_lockedInputs.begin(), m_lockedInputs.end(),
    [&keyImage](const auto x)
    {
        return x.keyImage == keyImage;
    });

    if (it != m_lockedInputs.end())
    {
        /* Set the spend height */
        it->spendHeight = spendHeight;

        /* Add to the spent inputs vector */
        m_spentInputs.push_back(*it);

        /* Remove from the locked vector */
        m_lockedInputs.erase(it);

        return;
    }
    
    /* Shouldn't happen */
    throw std::runtime_error("Could not find key image to remove!");
}

void SubWallet::markInputAsLocked(const Crypto::KeyImage keyImage)
{
    /* Find the input */
    auto it = std::find_if(m_unspentInputs.begin(), m_unspentInputs.end(),
    [&keyImage](const auto x)
    {
        return x.keyImage == keyImage;
    });

    /* Shouldn't happen */
    if (it == m_unspentInputs.end())
    {
        throw std::runtime_error("Could not find key image to lock!");
    }

    /* Add to the spent inputs vector */
    m_lockedInputs.push_back(*it);

    /* Remove from the unspent vector */
    m_unspentInputs.erase(it);
}

void SubWallet::removeForkedInputs(const uint64_t forkHeight)
{
    /* Both of these will be resolved by the wallet in time */
    m_lockedInputs.clear();
    m_unconfirmedIncomingAmounts.clear();

    /* Unspent inputs which we recieved in a block after the fork. Remove them. */
    auto it = std::remove_if(m_unspentInputs.begin(), m_unspentInputs.end(),
    [forkHeight](const auto input)
    {
        return input.blockHeight >= forkHeight;
    });

    if (it != m_unspentInputs.end())
    {
        m_unspentInputs.erase(it, m_unspentInputs.end());
    }

    /* If the input was spent after the fork height, but received before the
       fork height, then we keep it, but move it into the unspent vector */
    it = std::remove_if(m_spentInputs.begin(), m_spentInputs.end(),
    [&forkHeight, this](auto &input)
    {
        if (input.spendHeight >= forkHeight)
        {
            /* Reset spend height */
            input.spendHeight = 0;

            /* Readd to the unspent vector */
            m_unspentInputs.push_back(input);

            return true;
        }

        return false;
    });

    if (it != m_spentInputs.end())
    {
        m_spentInputs.erase(it, m_spentInputs.end());
    }
}

/* Cancelled transactions are transactions we sent, but got cancelled and not
   included in a block for some reason */
void SubWallet::removeCancelledTransactions(
    const std::unordered_set<Crypto::Hash> cancelledTransactions)
{
    /* Find the inputs used in the cancelled transactions */
    auto it = std::remove_if(m_lockedInputs.begin(), m_lockedInputs.end(),
    [&cancelledTransactions, this](auto &input)
    {
        if (cancelledTransactions.find(input.parentTransactionHash) != cancelledTransactions.end())
        {
            input.spendHeight = 0;

            /* Re-add the input to the unspent vector now it has been returned
               to our wallet */
            m_unspentInputs.push_back(input);

            return true;
        }

        return false;
    });

    /* Remove the inputs used in the cancelled tranactions */
    if (it != m_lockedInputs.end())
    {
        m_lockedInputs.erase(it, m_lockedInputs.end());
    }

    /* Find inputs that we 'received' in outgoing transfers (scanning our
       own sent transfer) and remove them */
    auto it2 = std::remove_if(m_unconfirmedIncomingAmounts.begin(), m_unconfirmedIncomingAmounts.end(),
    [&cancelledTransactions](auto &input)
    {
        return cancelledTransactions.find(input.parentTransactionHash) != cancelledTransactions.end();
    });

    if (it2 != m_unconfirmedIncomingAmounts.end())
    {
        m_unconfirmedIncomingAmounts.erase(it2, m_unconfirmedIncomingAmounts.end());
    }
}

std::vector<WalletTypes::TxInputAndOwner> SubWallet::getSpendableInputs(
    const uint64_t height) const
{
    std::vector<WalletTypes::TxInputAndOwner> inputs;

    for (const auto input : m_unspentInputs)
    {
        if (Utilities::isInputUnlocked(input.unlockTime, height))
        {
            inputs.emplace_back(input, m_publicSpendKey, m_privateSpendKey);
        }
    }

    return inputs;
}

uint64_t SubWallet::syncStartHeight() const
{
    return m_syncStartHeight;
}

uint64_t SubWallet::syncStartTimestamp() const
{
    return m_syncStartTimestamp;
}

void SubWallet::storeUnconfirmedIncomingInput(
    const WalletTypes::UnconfirmedInput input)
{
    m_unconfirmedIncomingAmounts.push_back(input);
}

void SubWallet::convertSyncTimestampToHeight(
    const uint64_t timestamp,
    const uint64_t height)
{
    if (m_syncStartTimestamp != 0)
    {
        m_syncStartTimestamp = timestamp;
        m_syncStartHeight = height;
    }
}

void SubWallet::pruneSpentInputs(const uint64_t pruneHeight)
{
    const uint64_t lenBeforePrune = m_spentInputs.size();

    const auto it = std::remove_if(m_spentInputs.begin(), m_spentInputs.end(),
    [&pruneHeight](const auto input)
    {
        return input.spendHeight <= pruneHeight;
    });

    if (it != m_spentInputs.end())
    {
        m_spentInputs.erase(it, m_spentInputs.end());
    }

    const uint64_t lenAfterPrune = m_spentInputs.size();

    const uint64_t difference = lenBeforePrune - lenAfterPrune;

    if (difference != 0)
    {
        Logger::logger.log(
            "Pruned " + std::to_string(difference) + " spent inputs from " + m_address,
            Logger::DEBUG,
            {Logger::SYNC}
        );
    }
}

std::vector<Crypto::KeyImage> SubWallet::getKeyImages() const
{
    std::vector<Crypto::KeyImage> result;

    const auto getKeyImages = [&result](const auto &vec)
    {
        std::transform(vec.begin(), vec.end(), std::back_inserter(result), [](const auto &input)
        {
            return input.keyImage;
        });
    };

    getKeyImages(m_unspentInputs);
    getKeyImages(m_lockedInputs);
    /* You may think we don't need to include the spent key images here, since
       we're using this method to check if an transaction was sent by us
       by comparing the key images, and a spent key image can of course not
       be used more than once.

       However, it is possible that a spent transaction gets orphaned, returns
       to our wallet, and is then spent again. If we did not include the spent
       key images, when we handle the fork and mark the inputs as unspent,
       we would not know about the key images of those inputs.

       Then, when we spend it again, we would not know it's our outgoing
       transaction. */
    getKeyImages(m_spentInputs);

    return result;
}

void SubWallet::fromJSON(const JSONValue &j)
{
    m_publicSpendKey.fromString(getStringFromJSON(j, "publicSpendKey"));

    m_privateSpendKey.fromString(getStringFromJSON(j, "privateSpendKey"));

    m_address = getStringFromJSON(j, "address");

    m_syncStartTimestamp = getUint64FromJSON(j, "syncStartTimestamp");

    for (const auto &x : getArrayFromJSON(j, "unspentInputs"))
    {
        WalletTypes::TransactionInput input;
        input.fromJSON(x);
        m_unspentInputs.push_back(input);
    }

    for (const auto &x : getArrayFromJSON(j, "lockedInputs"))
    {
        WalletTypes::TransactionInput input;
        input.fromJSON(x);
        m_lockedInputs.push_back(input);
    }

    for (const auto &x : getArrayFromJSON(j, "spentInputs"))
    {
        WalletTypes::TransactionInput input;
        input.fromJSON(x);
        m_spentInputs.push_back(input);
    }

    m_syncStartHeight = getUint64FromJSON(j, "syncStartHeight");

    m_isPrimaryAddress = getBoolFromJSON(j, "isPrimaryAddress");

    for (const auto &x : getArrayFromJSON(j, "unconfirmedIncomingAmounts"))
    {
        WalletTypes::UnconfirmedInput amount;
        amount.fromJSON(x);
        m_unconfirmedIncomingAmounts.push_back(amount);
    }
}

void SubWallet::toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
{
    writer.StartObject();

    writer.Key("publicSpendKey");
    m_publicSpendKey.toJSON(writer);

    writer.Key("privateSpendKey");
    m_privateSpendKey.toJSON(writer);

    writer.Key("address");
    writer.String(m_address);

    writer.Key("syncStartTimestamp");
    writer.Uint64(m_syncStartTimestamp);

    writer.Key("unspentInputs");
    writer.StartArray();
    for (const auto &input : m_unspentInputs)
    {
        input.toJSON(writer);
    }
    writer.EndArray();

    writer.Key("lockedInputs");
    writer.StartArray();
    for (const auto &input : m_lockedInputs)
    {
        input.toJSON(writer);
    }
    writer.EndArray();

    writer.Key("spentInputs");
    writer.StartArray();
    for (const auto &input : m_spentInputs)
    {
        input.toJSON(writer);
    }
    writer.EndArray();

    writer.Key("syncStartHeight");
    writer.Uint64(m_syncStartHeight);

    writer.Key("isPrimaryAddress");
    writer.Bool(m_isPrimaryAddress);

    writer.Key("unconfirmedIncomingAmounts");
    writer.StartArray();
    for (const auto &amount : m_unconfirmedIncomingAmounts)
    {
        amount.toJSON(writer);
    }
    writer.EndArray();

    writer.EndObject();
}
