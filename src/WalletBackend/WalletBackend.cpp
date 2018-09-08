// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

////////////////////////////////////////
#include <WalletBackend/WalletBackend.h>
////////////////////////////////////////

#include <crypto/random.h>

#include "CryptoNoteConfig.h"

#include <CryptoNoteCore/Account.h>
#include <CryptoNoteCore/CryptoNoteBasicImpl.h>

#include <cryptopp/aes.h>
#include <cryptopp/algparam.h>
#include <cryptopp/filters.h>
#include <cryptopp/modes.h>
#include <cryptopp/sha.h>
#include <cryptopp/pwdbased.h>

#include <fstream>

#include <iterator>

#include "json.hpp"

using json = nlohmann::json;

namespace WBackend {

//////////////////////////
/* NON MEMBER FUNCTIONS */
//////////////////////////

void prependMagicIdentifier(std::string &data, const uint8_t* magicIdentifier,
                            const size_t magicIdentifierSize)
{
    std::string tmp(magicIdentifier, magicIdentifier + magicIdentifierSize);

    data = tmp + data;
}

/////////////////////
/* CLASS FUNCTIONS */
/////////////////////

/* Imports a wallet from a mnemonic seed. Returns the wallet class,
           or an error. */
std::tuple<WalletError, WalletBackend> WalletBackend::importWalletFromSeed(
    const std::string mnemonicSeed, const std::string filename,
    const std::string password, const uint64_t scanHeight,
    const std::string daemonHost, const uint16_t daemonPort)
{
    WalletBackend wallet;

    return std::make_tuple(SUCCESS, wallet);
}

/* Imports a wallet from a private spend key and a view key. Returns
   the wallet class, or an error. */
std::tuple<WalletError, WalletBackend> WalletBackend::importWalletFromKeys(
    Crypto::SecretKey privateViewKey, Crypto::SecretKey privateSpendKey,
    const std::string filename, const std::string password,
    const uint64_t scanHeight, const std::string daemonHost,
    const uint16_t daemonPort)
{
    WalletBackend wallet;

    return std::make_tuple(SUCCESS, wallet);
}

/* Imports a view wallet from a private view key and an address.
   Returns the wallet class, or an error. */
std::tuple<WalletError, WalletBackend> WalletBackend::importViewWallet(
    const Crypto::SecretKey privateViewKey, const std::string address,
    const std::string filename, const std::string password,
    const uint64_t scanHeight, const std::string daemonHost,
    const uint16_t daemonPort)
{
    WalletBackend wallet;

    return std::make_tuple(SUCCESS, wallet);
}

/* Creates a new wallet with the given filename and password */
std::tuple<WalletError, WalletBackend> WalletBackend::createWallet(
    const std::string filename, const std::string password,
    const std::string daemonHost, const uint16_t daemonPort)
{
    WalletBackend wallet;

    CryptoNote::KeyPair spendKey;
    Crypto::SecretKey privateViewKey;
    Crypto::PublicKey publicViewKey;

    Crypto::generate_keys(spendKey.publicKey, spendKey.secretKey);

    CryptoNote::AccountBase::generateViewFromSpend(spendKey.secretKey,
                                                   privateViewKey,
                                                   publicViewKey);

    std::string address = CryptoNote::getAccountAddressAsStr(
        CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
        { spendKey.publicKey, publicViewKey }
    );

    wallet.m_filename = filename;
    wallet.m_password = password;

    wallet.m_privateViewKey = privateViewKey;
    wallet.m_privateSpendKeys.push_back(spendKey.secretKey);

    wallet.m_addresses.push_back(address);

    wallet.m_isViewWallet = false;

    wallet.save();

    return std::make_tuple(SUCCESS, wallet);
}

/* Opens a wallet already on disk with the given filename + password */
std::tuple<WalletError, WalletBackend> WalletBackend::openWallet(
    const std::string filename, const std::string password,
    const std::string daemonHost, const uint16_t daemonPort)
{
    WalletBackend wallet;

    /* Open in binary mode, since we have encrypted data */
    std::ifstream file(filename, std::ios::binary);

    if (!file)
    {
        return std::make_tuple(FILENAME_NON_EXISTENT, wallet);
    }

    /* Read file into a buffer */
    std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                             (std::istreambuf_iterator<char>()));

    /* Check the file is large enough for the wallet identifier so we don't
       go out of bounds */
    if (buffer.size() < sizeof(isAWalletIdentifier))
    {
        return std::make_tuple(NOT_A_WALLET_FILE, wallet);
    }

    /* Check the file has the correct prefix identifying it as a wallet file */
    for (size_t i = 0; i < sizeof(isAWalletIdentifier); i++)
    {
        if ((int)buffer[i] != isAWalletIdentifier[i])
        {
            return std::make_tuple(NOT_A_WALLET_FILE, wallet);
        }
    }

    /* Remove the prefix, don't need it anymore */
    buffer.erase(buffer.begin(), buffer.begin() + sizeof(isAWalletIdentifier));

    /* The salt we use for both PBKDF2, and AES decryption */
    CryptoPP::byte salt[16];

    /* Check the file is large enough for the salt */
    if (buffer.size() < sizeof(salt))
    {
        return std::make_tuple(WALLET_FILE_CORRUPTED, wallet);
    }

    /* Copy the salt to the salt array */
    std::copy(buffer.begin(), buffer.begin() + sizeof(salt), salt);

    /* Remove the salt, don't need it anymore */
    buffer.erase(buffer.begin(), buffer.begin() + sizeof(salt));

    /* The key we use for AES decryption, generated with PBKDF2 */
    CryptoPP::byte key[32];

    /* Using SHA256 as the algorithm */
    CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> pbkdf2;

    /* Generate the AES Key using pbkdf2 */
    pbkdf2.DeriveKey(key, sizeof(key), 0, (CryptoPP::byte *)password.data(),
                     password.size(), salt, sizeof(salt), PBKDF2_ITERATIONS);

    CryptoPP::AES::Decryption aesDecryption(key, sizeof(key));

    CryptoPP::CBC_Mode_ExternalCipher::Decryption cbcDecryption(
        aesDecryption, salt
    );

    /* This will store the encrypted data */
    std::string decryptedData;

    CryptoPP::StreamTransformationFilter stfDecryptor(
        cbcDecryption, new CryptoPP::StringSink(decryptedData)
    );

    /* Write the data to the AES decryptor stream */
    stfDecryptor.Put(reinterpret_cast<const CryptoPP::byte *>(buffer.data()),
                     buffer.size() + 1);

    stfDecryptor.MessageEnd();

    /* Check we've got space for the identifier */
    if (decryptedData.size() < sizeof(isCorrectPasswordIdentifier))
    {
        return std::make_tuple(WALLET_FILE_CORRUPTED, wallet);
    }

    for (size_t i = 0; i < sizeof(isCorrectPasswordIdentifier); i++)
    {
        if ((int)decryptedData[i] != isCorrectPasswordIdentifier[i])
        {
            return std::make_tuple(WRONG_PASSWORD, wallet);
        }
    }

    /* Remove the isCorrectPasswordIdentifier from the string */
    decryptedData.erase(decryptedData.begin(),
                        decryptedData.begin()
                      + sizeof(isCorrectPasswordIdentifier));

    /* Try and parse the decrypted json */
    auto error = wallet.fromJson(decryptedData);

    wallet.m_filename = filename;
    wallet.m_password = password;

    return std::make_tuple(error, wallet);
}

json WalletBackend::toJson() const
{
    json j = {
        {"privateViewKey", m_privateViewKey.data},
        {"isViewWallet", m_isViewWallet},
        {"addresses", m_addresses},
        {"privateSpendKeys", json::array()},
    };

    for (const auto &key : m_privateSpendKeys)
    {
        j["privateSpendKeys"].push_back(key.data);
    }

    return j;
}

WalletError WalletBackend::fromJson(std::string jsonStr)
{
    try
    {
        json j = json::parse(jsonStr);

        /* TODO: Is there a way to go directly to a C array from json? */
        auto tmpViewKey = j.at("privateViewKey").get<std::vector<uint8_t>>();

        for (size_t i = 0; i < tmpViewKey.size(); i++)
        {
            m_privateViewKey.data[i] = tmpViewKey[i];
        }

        m_isViewWallet = j.at("isViewWallet").get<bool>();
        m_addresses = j.at("addresses").get<std::vector<std::string>>();

        /* TODO: Is there a better way to do this? */
        /* Bless me father for I have sinned */
        auto tmpSpendKeys = j.at("privateSpendKey").get<std::vector<
            std::array<uint8_t, sizeof(Crypto::SecretKey::data)>
        >>();

        for (const auto &key : tmpSpendKeys)
        {
            Crypto::SecretKey tmp;

            for (size_t i = 0; i < key.size(); i++)
            {
                tmp.data[i] = key[i];
            }

            m_privateSpendKeys.push_back(tmp);
        }
    }
    catch (const json::type_error &e)
    {
        return WALLET_FILE_CORRUPTED;
    }
    catch (const json::out_of_range &e)
    {
        return WALLET_FILE_CORRUPTED;
    }
    catch (const json::parse_error &e)
    {
        return WALLET_FILE_CORRUPTED;
    }

    return SUCCESS;
}

WalletError WalletBackend::save() const
{
    /* Serialize wallet to json, and get it as a string */
    std::string walletData = toJson().dump();

    /* Add an identifier to the start of the string so we can verify the wallet
       has been correctly decrypted */
    prependMagicIdentifier(walletData, isAWalletIdentifier,
                           sizeof(isAWalletIdentifier));

    /* The key we use for AES encryption, generated with PBKDF2 */
    CryptoPP::byte key[32];

    /* The salt we use for both PBKDF2, and AES Encryption */
    CryptoPP::byte salt[16];

    /* Generate 16 random bytes for the salt */
    Crypto::generate_random_bytes(16, salt);

    /* Using SHA256 as the algorithm */
    CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> pbkdf2;

    /* Generate the AES Key using pbkdf2 */
    pbkdf2.DeriveKey(key, sizeof(key), 0, (CryptoPP::byte *)m_password.data(),
                     m_password.size(), salt, sizeof(salt), PBKDF2_ITERATIONS);

    CryptoPP::AES::Encryption aesEncryption(key, sizeof(key));

    /* Using the CBC mode of AES encryption */
    CryptoPP::CBC_Mode_ExternalCipher::Encryption cbcEncryption(
        aesEncryption, salt
    );

    /* This will store the encrypted data */
    std::string encryptedData;

    CryptoPP::StreamTransformationFilter stfEncryptor(
        cbcEncryption, new CryptoPP::StringSink(encryptedData)
    );

    /* Write the data to the AES stream */
    stfEncryptor.Put(reinterpret_cast<const CryptoPP::byte *>(walletData.c_str()),
                     walletData.length() + 1);

    stfEncryptor.MessageEnd();

    std::ofstream file(m_filename);

    if (!file)
    {
        return FAILED_TO_SAVE_WALLET;
    }

    /* Get the isAWalletIdentifier array as a string */
    std::string walletPrefix = std::string(
        isAWalletIdentifier, isAWalletIdentifier + sizeof(isAWalletIdentifier)
    );

    /* Get the salt array as a string */
    std::string saltString = std::string(salt, salt + sizeof(salt));

    /* Write the isAWalletIdentifier to the file, so when we open it we can
       verify that it is a wallet file */
    file << walletPrefix;

    /* Write the salt to the file, so we can use it to unencrypt the file
       later. Note that the salt is unencrypted. */
    file << saltString;

    /* Write the encrypted wallet data to the file */
    file << encryptedData;

    return SUCCESS;
}

} // namespace WalletBackend
