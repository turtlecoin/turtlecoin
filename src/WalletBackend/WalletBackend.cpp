// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

////////////////////////////////////////
#include <WalletBackend/WalletBackend.h>
////////////////////////////////////////

#include <config/CryptoNoteConfig.h>

#include <crypto/random.h>

#include <CryptoNoteCore/Account.h>
#include <CryptoNoteCore/CryptoNoteBasicImpl.h>

#include <cryptopp/aes.h>
#include <cryptopp/algparam.h>
#include <cryptopp/filters.h>
#include <cryptopp/modes.h>
#include <cryptopp/sha.h>
#include <cryptopp/pwdbased.h>

#include <fstream>
#include <future>

#include <iterator>

#include "json.hpp"

#include <Logging/LoggerManager.h>

#include <Mnemonics/Mnemonics.h>

#include <WalletBackend/JsonSerialization.h>

using json = nlohmann::json;

//////////////////////////
/* NON MEMBER FUNCTIONS */
//////////////////////////

/* Anonymous namespace so it doesn't clash with anything else */
namespace {

/* Check data has the magic indicator from first : last, and remove it if
   it does. Else, return an error depending on where we failed */
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

/* Check the wallet filename for the new wallet to be created is valid */
WalletError checkNewWalletFilename(std::string filename)
{
    /* Check the file doesn't exist */
    if (std::ifstream(filename))
    {
        return WALLET_FILE_ALREADY_EXISTS;
    }

    /* Check we can open the file */
    if (!std::ofstream(filename))
    {
        return INVALID_WALLET_FILENAME;
    }
    
    return SUCCESS;
}

} // namespace

///////////////////////////////////
/* CONSTRUCTORS / DECONSTRUCTORS */
///////////////////////////////////

/* Constructor */
WalletBackend::WalletBackend()
{
    m_logManager = std::make_shared<Logging::LoggerManager>();

    m_logger = std::make_shared<Logging::LoggerRef>(
        *m_logManager, "WalletBackend"
    );

    /* Remember to call initializeAfterLoad() to initialize the daemon - 
    we can't do it here since we don't have the host/port, and the json
    serialization uses the default constructor */
}

/* Deconstructor */
WalletBackend::~WalletBackend()
{
    /* Save, but only if the non default constructor was used - else things
       will be uninitialized, and crash */
    if (m_daemon != nullptr)
    {
        save();
    }
}

/* Move constructor */
WalletBackend::WalletBackend(WalletBackend && old)
{
    /* Call the move assignment operator */
    *this = std::move(old);
}

/* Move assignment operator */
WalletBackend & WalletBackend::operator=(WalletBackend && old)
{
    m_filename = old.m_filename;
    m_password = old.m_password;
    m_privateViewKey = old.m_privateViewKey;
    m_isViewWallet = old.m_isViewWallet;
    m_logManager = old.m_logManager;
    m_daemon = old.m_daemon;
    m_subWallets = old.m_subWallets;
    m_walletSynchronizer = old.m_walletSynchronizer;

    /* Invalidate the old pointers */
    old.m_logManager = nullptr;
    old.m_daemon = nullptr;
    old.m_logger = nullptr;
    old.m_subWallets = nullptr;
    old.m_walletSynchronizer = nullptr;

    return *this;
}

/* Constructor */
WalletBackend::WalletBackend(std::string filename, std::string password,
                             Crypto::SecretKey privateSpendKey,
                             Crypto::SecretKey privateViewKey,
                             bool isViewWallet, uint64_t scanHeight,
                             bool newWallet, std::string daemonHost,
                             uint16_t daemonPort) :
    m_filename(filename),
    m_password(password),
    m_privateViewKey(privateViewKey),
    m_isViewWallet(isViewWallet)
{
    m_logManager = std::make_shared<Logging::LoggerManager>();

    m_logger = std::make_shared<Logging::LoggerRef>(
        *m_logManager, "WalletBackend"
    );

    m_daemon = std::make_shared<CryptoNote::NodeRpcProxy>(
        daemonHost, daemonPort, m_logger->getLogger()
    );

    /* Generate the address from the two private keys */
    std::string address = addressFromPrivateKeys(privateSpendKey,
                                                 privateViewKey);

    m_subWallets = std::make_shared<SubWallets>(
        privateSpendKey, address, scanHeight, newWallet
    );
}

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
    /* Check the filename is valid */
    WalletError error = checkNewWalletFilename(filename);

    if (error)
    {
        return std::make_tuple(error, WalletBackend());
    }

    std::string mnemonicError;

    Crypto::SecretKey privateSpendKey;

    /* Convert the mnemonic into a private spend key */
    std::tie(mnemonicError, privateSpendKey)
        = Mnemonics::MnemonicToPrivateKey(mnemonicSeed);

    /* TODO: Return a more informative error */
    if (!mnemonicError.empty())
    {
        return std::make_tuple(INVALID_MNEMONIC, WalletBackend());
    }

    Crypto::SecretKey privateViewKey;

    /* Derive the private view key from the private spend key */
    CryptoNote::AccountBase::generateViewFromSpend(privateSpendKey,
                                                   privateViewKey);

    /* Just defining here so it's more obvious what we're doing in the
       constructor */
    bool newWallet = false;
    bool isViewWallet = false;

    WalletBackend wallet(
        filename, password, privateSpendKey, privateViewKey, isViewWallet,
        scanHeight, newWallet, daemonHost, daemonPort
    );

    error = wallet.init();

    if (error)
    {
        return std::make_tuple(error, WalletBackend());
    }

    /* Save to disk */
    error = wallet.save();

    return std::make_tuple(error, std::move(wallet));
}

/* Imports a wallet from a private spend key and a view key. Returns
   the wallet class, or an error. */
std::tuple<WalletError, WalletBackend> WalletBackend::importWalletFromKeys(
    Crypto::SecretKey privateSpendKey, Crypto::SecretKey privateViewKey,
    const std::string filename, const std::string password,
    const uint64_t scanHeight, const std::string daemonHost,
    const uint16_t daemonPort)
{
    /* Check the filename is valid */
    WalletError error = checkNewWalletFilename(filename);

    if (error)
    {
        return std::make_tuple(error, WalletBackend());
    }

    /* Just defining here so it's more obvious what we're doing in the
       constructor */
    bool newWallet = false;
    bool isViewWallet = false;

    auto wallet = WalletBackend(
        filename, password, privateSpendKey, privateViewKey, isViewWallet,
        scanHeight, newWallet, daemonHost, daemonPort
    );

    error = wallet.init();

    if (error)
    {
        return std::make_tuple(error, WalletBackend());
    }

    /* Save to disk */
    error = wallet.save();

    return std::make_tuple(error, std::move(wallet));
}

/* Imports a view wallet from a private view key and an address.
   Returns the wallet class, or an error. */
/* TODO: What do we need the address parameter for / or how do we use it? */
std::tuple<WalletError, WalletBackend> WalletBackend::importViewWallet(
    const Crypto::SecretKey privateViewKey, const std::string address,
    const std::string filename, const std::string password,
    const uint64_t scanHeight, const std::string daemonHost,
    const uint16_t daemonPort)
{
    /* Check the filename is valid */
    WalletError error = checkNewWalletFilename(filename);

    if (error)
    {
        return std::make_tuple(error, WalletBackend());
    }

    /* Just defining here so it's more obvious what we're doing in the
       constructor */
    bool newWallet = false;
    bool isViewWallet = true;

    auto wallet = WalletBackend(
        filename, password, CryptoNote::NULL_SECRET_KEY, privateViewKey,
        isViewWallet, scanHeight, newWallet, daemonHost, daemonPort
    );

    error = wallet.init();

    if (error)
    {
        return std::make_tuple(error, WalletBackend());
    }

    /* Save to disk */
    error = wallet.save();

    return std::make_tuple(error, std::move(wallet));
}

/* Creates a new wallet with the given filename and password */
std::tuple<WalletError, WalletBackend> WalletBackend::createWallet(
    const std::string filename, const std::string password,
    const std::string daemonHost, const uint16_t daemonPort)
{
    /* Check the filename is valid */
    WalletError error = checkNewWalletFilename(filename);

    if (error)
    {
        return std::make_tuple(error, WalletBackend());
    }

    CryptoNote::KeyPair spendKey;
    Crypto::SecretKey privateViewKey;
    Crypto::PublicKey publicViewKey;

    /* Generate a spend key */
    Crypto::generate_keys(spendKey.publicKey, spendKey.secretKey);

    /* Derive the view key from the spend key */
    CryptoNote::AccountBase::generateViewFromSpend(
        spendKey.secretKey, privateViewKey, publicViewKey
    );

    /* Just defining here so it's more obvious what we're doing in the
       constructor */
    bool newWallet = true;
    bool isViewWallet = false;
    uint64_t scanHeight = 0;

    auto wallet = WalletBackend(
        filename, password, spendKey.secretKey, privateViewKey, isViewWallet,
        scanHeight, newWallet, daemonHost, daemonPort
    );

    error = wallet.init();

    if (error)
    {
        return std::make_tuple(error, WalletBackend());
    }

    /* Save to disk */
    error = wallet.save();

    return std::make_tuple(error, std::move(wallet));
}

/* Opens a wallet already on disk with the given filename + password */
std::tuple<WalletError, WalletBackend> WalletBackend::openWallet(
    const std::string filename, const std::string password,
    const std::string daemonHost, const uint16_t daemonPort)
{
    /* Open in binary mode, since we have encrypted data */
    std::ifstream file(filename, std::ios::binary);

    /* Check we successfully opened the file */
    if (!file)
    {
        return std::make_tuple(FILENAME_NON_EXISTENT, WalletBackend());
    }

    /* Read file into a buffer */
    std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                             (std::istreambuf_iterator<char>()));

    /* Check that the decrypted data has the 'isAWallet' identifier,
       and remove it it does. If it doesn't, return an error. */
    WalletError error = hasMagicIdentifier(
        buffer, isAWalletIdentifier.begin(), isAWalletIdentifier.end(),
        NOT_A_WALLET_FILE, NOT_A_WALLET_FILE
    );

    if (error)
    {
        return std::make_tuple(error, WalletBackend());
    }

    /* The salt we use for both PBKDF2, and AES decryption */
    CryptoPP::byte salt[16];

    /* Check the file is large enough for the salt */
    if (buffer.size() < sizeof(salt))
    {
        return std::make_tuple(WALLET_FILE_CORRUPTED, WalletBackend());
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
        return std::make_tuple(error, WalletBackend());
    }

    WalletBackend wallet = json::parse(decryptedData);

    /* Since json::parse() uses the default constructor, the node, filename,
       and password won't be initialized. */
    error = wallet.initializeAfterLoad(
        filename, password, daemonHost, daemonPort
    );

    return std::make_tuple(error, std::move(wallet));
}

WalletError WalletBackend::initializeAfterLoad(std::string filename,
    std::string password, std::string daemonHost, uint16_t daemonPort)
{
    m_filename = filename;
    m_password = password;

    m_daemon = std::make_shared<CryptoNote::NodeRpcProxy>(
        daemonHost, daemonPort, m_logger->getLogger()
    );

    return init();
}

WalletError WalletBackend::init()
{
    if (m_daemon == nullptr)
    {
        throw std::runtime_error("Daemon has not been initialized!");
    }

    std::cout << "Initializing daemon, this may hang..." << std::endl;

    std::promise<std::error_code> errorPromise;
    std::future<std::error_code> error = errorPromise.get_future();

    auto callback = [&errorPromise](std::error_code e) 
    {
        errorPromise.set_value(e);
    };

    m_daemon->init(callback);

    std::future<WalletError> initDaemon = std::async(std::launch::async,
                                                    [&error]
    {
        if (error.get())
        {
            return FAILED_TO_INIT_DAEMON;
        }
        else
        {
            return SUCCESS;
        }
    });


    /* Wait for the daemon to init */
    /* TODO: This can hang - can't do it in a std::future since that hangs
       when going of out scope */
    WalletError result = initDaemon.get();

    /* Init the wallet synchronizer if it hasn't been loaded from the wallet
       file */
    if (m_walletSynchronizer == nullptr)
    {
        uint64_t startHeight;
        uint64_t startTimestamp;

        std::tie(startHeight, startTimestamp) = m_subWallets->getMinInitialSyncStart();

        m_walletSynchronizer = std::make_shared<WalletSynchronizer>(
            m_daemon, 
            startHeight,
            startTimestamp,
            m_privateViewKey
        );
    }
    /* If it has, just set the daemon */
    else
    {
        m_walletSynchronizer->m_daemon = m_daemon;
    }

    m_walletSynchronizer->m_subWallets = m_subWallets;

    /* Launch the wallet sync process in a background thread */
    m_walletSynchronizer->start();

    std::cout << "Daemon initialization completed!" << std::endl;

    return result;
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
        return INVALID_WALLET_FILENAME;
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
