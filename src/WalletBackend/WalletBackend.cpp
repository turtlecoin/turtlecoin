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

#include <Mnemonics/Mnemonics.h>

#include <WalletBackend/JsonSerialization.h>

using json = nlohmann::json;

//////////////////////////
/* NON MEMBER FUNCTIONS */
//////////////////////////

/* Anonymous namespace so it doesn't clash with anything else */
namespace {

template <class Iterator, class Buffer>
WalletError hasMagicIdentifier(Buffer &data, Iterator first, Iterator last,
                               WalletError tooSmallError,
                               WalletError wrongIdentifierError)
{
    size_t identifierSize = std::distance(first, last);

    /* Check we've got space for the identifier */
    if (data.size() < identifierSize)
    {
        return tooSmallError;
    }

    for (size_t i = 0; i < identifierSize; i++)
    {
        if ((int)data[i] != *(first + i))
        {
            return wrongIdentifierError;
        }
    }

    /* Remove the identifier from the string */
    data.erase(data.begin(), data.begin() + identifierSize);

    return SUCCESS;
}

/* Generates a public address from the given private keys */
std::string addressFromPrivateKeys(const Crypto::SecretKey &privateSpendKey,
                                   const Crypto::SecretKey &privateViewKey)
{
    Crypto::PublicKey publicSpendKey;
    Crypto::PublicKey publicViewKey;

    Crypto::secret_key_to_public_key(privateSpendKey, publicSpendKey);
    Crypto::secret_key_to_public_key(privateViewKey, publicViewKey);

    return CryptoNote::getAccountAddressAsStr(
        CryptoNote::parameters::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
        { publicSpendKey, publicViewKey }
    );
}

} // namespace

/////////////////////
/* CLASS FUNCTIONS */
/////////////////////

/* Have to provide definition of the static member in C++11...
   https://stackoverflow.com/a/8016853/8737306 */
constexpr std::array<uint8_t, 64> WalletBackend::isAWalletIdentifier;

constexpr std::array<uint8_t, 26> WalletBackend::isCorrectPasswordIdentifier;

/* Imports a wallet from a mnemonic seed. Returns the wallet class,
           or an error. */
std::tuple<WalletError, WalletBackend> WalletBackend::importWalletFromSeed(
    const std::string mnemonicSeed, const std::string filename,
    const std::string password, const uint64_t scanHeight,
    const std::string daemonHost, const uint16_t daemonPort)
{
    WalletBackend wallet;

    std::string error;

    Crypto::SecretKey privateSpendKey;
    Crypto::SecretKey privateViewKey;
    
    /* Convert the mnemonic into a private spend key */
    std::tie(error, privateSpendKey) = Mnemonics::MnemonicToPrivateKey(mnemonicSeed);

    /* TODO: Return a more informative error */
    if (!error.empty())
    {
        return std::make_tuple(INVALID_MNEMONIC, wallet);
    }

    /* Derive the private view key from the private spend key */
    CryptoNote::AccountBase::generateViewFromSpend(privateSpendKey,
                                                   privateViewKey);

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

    WalletBackend wallet(filename, password, spendKey.secretKey,
                         privateViewKey, false);

    /* Saving can fail */
    WalletError error = wallet.save();

    return std::make_tuple(error, wallet);
}

/* Opens a wallet already on disk with the given filename + password */
std::tuple<WalletError, WalletBackend> WalletBackend::openWallet(
    const std::string filename, const std::string password,
    const std::string daemonHost, const uint16_t daemonPort)
{
    WalletBackend wallet;
    WalletError error;

    /* Open in binary mode, since we have encrypted data */
    std::ifstream file(filename, std::ios::binary);

    /* Check we successfully opened the file */
    if (!file)
    {
        return std::make_tuple(FILENAME_NON_EXISTENT, wallet);
    }

    /* Read file into a buffer */
    std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                             (std::istreambuf_iterator<char>()));

    /* Check that the decrypted data has the 'isAWallet' identifier,
       and remove it it does. If it doesn't, return an error. */
    error = hasMagicIdentifier(
        buffer, isAWalletIdentifier.begin(), isAWalletIdentifier.end(),
        NOT_A_WALLET_FILE, NOT_A_WALLET_FILE
    );

    if (error)
    {
        return std::make_tuple(error, wallet);
    }

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

    /* Intialize aesDecryption with the AES Key */
    CryptoPP::AES::Decryption aesDecryption(key, sizeof(key));

    /* Using CBC encryption, pass in the salt */
    CryptoPP::CBC_Mode_ExternalCipher::Decryption cbcDecryption(
        aesDecryption, salt
    );

    /* This will store the decrypted data */
    std::string decryptedData;

    /* Stream the decrypted data into the decryptedData string */
    CryptoPP::StreamTransformationFilter stfDecryptor(
        cbcDecryption, new CryptoPP::StringSink(decryptedData)
    );

    /* Write the data to the AES decryptor stream */
    stfDecryptor.Put(reinterpret_cast<const CryptoPP::byte *>(buffer.data()),
                     buffer.size());

    stfDecryptor.MessageEnd();

    /* Check that the decrypted data has the 'isCorrectPassword' identifier,
       and remove it it does. If it doesn't, return an error. */
    error = hasMagicIdentifier(
        decryptedData, isCorrectPasswordIdentifier.begin(),
        isCorrectPasswordIdentifier.end(), WALLET_FILE_CORRUPTED,
        WRONG_PASSWORD
    );

    if (error)
    {
        return std::make_tuple(error, wallet);
    }

    wallet = json::parse(decryptedData);

    wallet.m_filename = filename;
    wallet.m_password = password;

    return std::make_tuple(error, wallet);
}

WalletBackend::WalletBackend(std::string filename, std::string password,
                             Crypto::SecretKey privateSpendKey,
                             Crypto::SecretKey privateViewKey,
                             bool isViewWallet) :
    m_filename(filename),
    m_password(password),
    m_privateViewKey(privateViewKey),
    m_isViewWallet(isViewWallet)
{
}

WalletError WalletBackend::save() const
{
    /* Add an identifier to the start of the string so we can verify the wallet
       has been correctly decrypted */
    std::string identiferAsString(isCorrectPasswordIdentifier.begin(),
                                  isCorrectPasswordIdentifier.end());

    /* Serialize wallet to json */
    json walletJson = *this;

    /* Add magic identifier, and get json as a string */
    std::string walletData = identiferAsString + walletJson.dump();

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
    stfEncryptor.Put(
        reinterpret_cast<const CryptoPP::byte *>(walletData.c_str()),
        walletData.length()
    );

    stfEncryptor.MessageEnd();

    std::ofstream file(m_filename);

    if (!file)
    {
        return FAILED_TO_SAVE_WALLET;
    }

    /* Get the isAWalletIdentifier array as a string */
    std::string walletPrefix = std::string(isAWalletIdentifier.begin(),
                                           isAWalletIdentifier.end());

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
