// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

///////////////////////////
#include <zedwallet/Open.h>
///////////////////////////

#include <boost/algorithm/string.hpp>

#include <CryptoNoteCore/Account.h>
#include <CryptoNoteCore/CryptoNoteBasicImpl.h>

#include <Mnemonics/Mnemonics.h>

#include <Wallet/WalletErrors.h>

#include <zedwallet/ColouredMsg.h>
#include <zedwallet/CommandImplementations.h>
#include <zedwallet/Transfer.h>
#include <zedwallet/Types.h>
#include <zedwallet/PasswordContainer.h>
#include <zedwallet/WalletConfig.h>

std::shared_ptr<WalletInfo> createViewWallet(CryptoNote::WalletGreen &wallet)
{
    Crypto::SecretKey privateViewKey = getPrivateKey("Private View Key: ");

    std::string address;

    while (true)
    {
        std::cout << "Public " << WalletConfig::ticker << " address: ";

        std::getline(std::cin, address);
        boost::algorithm::trim(address);

        if (parseStandardAddress(address, true))
        {
            break;
        }
    }

    const std::string walletFileName = getNewWalletFileName();

    const std::string msg = "Give your new wallet a password: ";
    const std::string walletPass = getWalletPassword(true, msg);

    const uint64_t scanHeight = getScanHeight();

    wallet.createViewWallet(walletFileName, walletPass, address,
                            privateViewKey, scanHeight, false);

    std::cout << std::endl << InformationMsg("Your view wallet ")
              << InformationMsg(address)
              << InformationMsg(" has been successfully imported!")
              << std::endl << std::endl;

    viewWalletMsg();

    return std::make_shared<WalletInfo>(walletFileName, walletPass, 
                                        address, true, wallet);
}

std::shared_ptr<WalletInfo> importWallet(CryptoNote::WalletGreen &wallet)
{
    const Crypto::SecretKey privateSpendKey
        = getPrivateKey("Private Spend Key: ");

    const Crypto::SecretKey privateViewKey
        = getPrivateKey("Private View Key: ");

    return importFromKeys(wallet, privateSpendKey, privateViewKey);
}

std::shared_ptr<WalletInfo> mnemonicImportWallet(CryptoNote::WalletGreen
                                                 &wallet)
{
    std::string mnemonicPhrase;

    Crypto::SecretKey privateSpendKey;
    Crypto::SecretKey privateViewKey;

    while (true)
    {
        std::cout << "Mnemonic Phrase (25 words): ";
        std::getline(std::cin, mnemonicPhrase);
        boost::algorithm::trim(mnemonicPhrase);
        
        std::string error;

        std::tie(error, privateSpendKey)
            = Mnemonics::MnemonicToPrivateKey(mnemonicPhrase);

        if (!error.empty())
        {
            std::cout << WarningMsg(error) << std::endl;
        }
        else
        {
            break;
        }
    }

    CryptoNote::AccountBase::generateViewFromSpend(privateSpendKey, 
                                                   privateViewKey);

    return importFromKeys(wallet, privateSpendKey, privateViewKey);
}

std::shared_ptr<WalletInfo> importFromKeys(CryptoNote::WalletGreen &wallet,
                                           Crypto::SecretKey privateSpendKey, 
                                           Crypto::SecretKey privateViewKey)
{
    const std::string walletFileName = getNewWalletFileName();

    const std::string msg = "Give your new wallet a password: ";
    const std::string walletPass = getWalletPassword(true, msg);

    const uint64_t scanHeight = getScanHeight();

    connectingMsg();

    wallet.initializeWithViewKey(walletFileName, walletPass, privateViewKey);

    const std::string walletAddress = wallet.createAddress(
        privateSpendKey, scanHeight, false
    );

    std::cout << std::endl << InformationMsg("Your wallet ")
              << InformationMsg(walletAddress)
              << InformationMsg(" has been successfully imported!")
              << std::endl << std::endl;

    return std::make_shared<WalletInfo>(walletFileName, walletPass, 
                                        walletAddress, false, wallet);
}

std::shared_ptr<WalletInfo> generateWallet(CryptoNote::WalletGreen &wallet)
{
    const std::string walletFileName = getNewWalletFileName();

    const std::string msg = "Give your new wallet a password: ";
    const std::string walletPass = getWalletPassword(true, msg);

    CryptoNote::KeyPair spendKey;
    Crypto::SecretKey privateViewKey;

    Crypto::generate_keys(spendKey.publicKey, spendKey.secretKey);
    CryptoNote::AccountBase::generateViewFromSpend(spendKey.secretKey,
                                                   privateViewKey);

    wallet.initializeWithViewKey(walletFileName, walletPass, privateViewKey);

    const std::string walletAddress = wallet.createAddress(
        spendKey.secretKey, 0, true
    );

    promptSaveKeys(wallet);

    std::cout << WarningMsg("If you lose these your wallet cannot be ")
              << WarningMsg("recreated!")
              << std::endl << std::endl;

    return std::make_shared<WalletInfo>(walletFileName, walletPass,
                                        walletAddress, false, wallet);
}

Maybe<std::shared_ptr<WalletInfo>> openWallet(CryptoNote::WalletGreen &wallet,
                                              Config &config)
{
    const std::string walletFileName = getExistingWalletFileName(config);

    bool initial = true;

    while (true)
    {
        std::string walletPass;

        /* Only use the command line pass once, otherwise we will infinite
           loop if it is incorrect */
        if (initial && config.passGiven)
        {
            walletPass = config.walletPass;
        }
        else
        {
            walletPass = getWalletPassword(false, "Enter password: ");
        }

        initial = false;

        connectingMsg();

        try
        {
            wallet.load(walletFileName, walletPass);

            const std::string walletAddress = wallet.getAddress(0);
            
            const Crypto::SecretKey privateSpendKey
                = wallet.getAddressSpendKey(0).secretKey;

            if (privateSpendKey == CryptoNote::NULL_SECRET_KEY)
            {
                std::cout << std::endl
                          << InformationMsg("Your view only wallet ")
                          << InformationMsg(walletAddress)
                          << InformationMsg(" has been successfully opened!")
                          << std::endl << std::endl;

                viewWalletMsg();

                return Just<std::shared_ptr<WalletInfo>>
                           (std::make_shared<WalletInfo>(walletFileName,
                                                         walletPass, 
                                                         walletAddress,
                                                         true, 
                                                         wallet));
            }
            else
            {
                std::cout << std::endl
                          << InformationMsg("Your wallet ")
                          << InformationMsg(walletAddress)
                          << InformationMsg(" has been successfully opened!")
                          << std::endl << std::endl;

                return Just<std::shared_ptr<WalletInfo>>
                           (std::make_shared<WalletInfo>(walletFileName,
                                                         walletPass, 
                                                         walletAddress,
                                                         false, 
                                                         wallet));
            }
 
            return Just<std::shared_ptr<WalletInfo>>
                       (std::make_shared<WalletInfo>(walletFileName,
                                                     walletPass, 
                                                     walletAddress,
                                                     false,
                                                     wallet));
        }
        catch (const std::system_error& e)
        {
            bool handled = false;

            switch (e.code().value())
            {
                case CryptoNote::error::WRONG_PASSWORD:
                {
                    std::cout << WarningMsg("Incorrect password! Try again.")
                              << std::endl;

                    handled = true;

                    break;
                }
                case CryptoNote::error::WRONG_VERSION:
                {
                    std::stringstream msg;

                    msg << "Could not open wallet file! It doesn't appear "
                        << "to be a valid wallet!" << std::endl
                        << "Ensure you are opening a wallet file, and the "
                        << "file has not gotten corrupted." << std::endl
                        << "Try reimporting via keys, and always close "
                        << WalletConfig::walletName << " with the exit "
                        << "command to prevent corruption." << std::endl;

                    std::cout << WarningMsg(msg.str()) << std::endl;

                    std::cout << "Returning to selection screen..."
                              << std::endl << std::endl;

                    return Nothing<std::shared_ptr<WalletInfo>>();

                    /* You never know... ;) */
                    break;
                }
            }

            if (handled)
            {
                continue;
            }

            const std::string alreadyOpenMsg =
                "MemoryMappedFile::open: The process cannot access the file "
                "because it is being used by another process.";

            const std::string errorMsg = e.what();
                
            /* The message actually has a \r\n on the end but i'd prefer to
               keep just the raw string in the source so check the it starts
               with instead */
            if (boost::starts_with(errorMsg, alreadyOpenMsg))
            {
                std::cout << WarningMsg("Could not open wallet! It is already "
                                        "open in another process.")
                          << std::endl
                          << WarningMsg("Check with a task manager that you "
                                        "don't have ")
                          << WalletConfig::walletName
                          << WarningMsg(" open twice.")
                          << std::endl
                          << WarningMsg("Also check you don't have another "
                                        "wallet program open, such as a GUI "
                                        "wallet or ")
                          << WarningMsg(WalletConfig::walletdName)
                          << WarningMsg(".")
                          << std::endl << std::endl;

                std::cout << "Returning to selection screen..." << std::endl
                          << std::endl;

                return Nothing<std::shared_ptr<WalletInfo>>();
            }
            else
            {
                std::cout << "Unexpected error: " << errorMsg << std::endl;
                std::cout << "Please report this error message and what "
                          << "you did to cause it." << std::endl << std::endl;

                std::cout << "Returning to selection screen..." << std::endl
                          << std::endl;

                return Nothing<std::shared_ptr<WalletInfo>>();
            }
        }
    }
}

Crypto::SecretKey getPrivateKey(std::string msg)
{
    const size_t privateKeyLen = 64;
    size_t size;

    std::string privateKeyString;
    Crypto::Hash privateKeyHash;
    Crypto::SecretKey privateKey;
    Crypto::PublicKey publicKey;

    while (true)
    {
        std::cout << msg;

        std::getline(std::cin, privateKeyString);
        boost::algorithm::trim(privateKeyString);

        if (privateKeyString.length() != privateKeyLen)
        {
            std::cout << WarningMsg("Invalid private key, should be 64 "
                                    "characters! Try again.") << std::endl;
            continue;
        }
        else if (!Common::fromHex(privateKeyString, &privateKeyHash, 
                  sizeof(privateKeyHash), size)
               || size != sizeof(privateKeyHash))
        {
            std::cout << WarningMsg("Invalid private key, failed to parse! "
                                    "Ensure you entered it correctly.")
                      << std::endl;
            continue;
        }

        privateKey = *(struct Crypto::SecretKey *) &privateKeyHash;

        /* Just used for verification purposes before we pass it to
           walletgreen */
        if (!Crypto::secret_key_to_public_key(privateKey, publicKey))
        {
            std::cout << "Invalid private key, failed to parse! Ensure "
                         "you entered it correctly." << std::endl;
            continue;
        }

        return privateKey;
    }
}

std::string getExistingWalletFileName(Config &config)
{
    bool initial = true;

    std::string walletName;

    while (true)
    {
        /* Only use wallet file once in case it is incorrect */
        if (config.walletGiven && initial)
        {
            walletName = config.walletFile;
        }
        else
        {
            std::cout << "What is the name of the wallet you want to open?: ";
            std::getline(std::cin, walletName);
        }

        initial = false;

        const std::string walletFileName = walletName + ".wallet";

        if (walletName == "")
        {
            std::cout << WarningMsg("Wallet name can't be blank! Try again.")
                      << std::endl;
        }
        /* Allow people to enter wallet name with or without file extension */
        else if (boost::filesystem::exists(walletName))
        {
            return walletName;
        }
        else if (boost::filesystem::exists(walletFileName))
        {
            return walletFileName;
        }
        else
        {
            std::cout << WarningMsg("A wallet with the filename ")
                      << InformationMsg(walletName)
                      << WarningMsg(" or ")
                      << InformationMsg(walletFileName)
                      << WarningMsg(" doesn't exist!")
                      << std::endl
                      << "Ensure you entered your wallet name correctly."
                      << std::endl;
        }
    }
}

std::string getNewWalletFileName()
{
    std::string walletName;

    while (true)
    {
        std::cout << "What would you like to call your new wallet?: ";
        std::getline(std::cin, walletName);

        const std::string walletFileName = walletName + ".wallet";

        if (boost::filesystem::exists(walletFileName))
        {
            std::cout << WarningMsg("A wallet with the filename " 
                                  + walletFileName + " already exists!")
                      << std::endl
                      << "Try another name." << std::endl;
        }
        else if (walletName == "")
        {
            std::cout << WarningMsg("Wallet name can't be blank! Try again.")
                      << std::endl;
        }
        else
        {
            return walletFileName;
        }
    }
}

std::string getWalletPassword(bool verifyPwd, std::string msg)
{
    Tools::PasswordContainer pwdContainer;
    pwdContainer.read_password(verifyPwd, msg);
    return pwdContainer.password();
}

uint64_t getScanHeight()
{
    while (true)
    {
        std::cout << "What height would you like to begin scanning "
                  << "your wallet from?"
                  << std::endl
                  << "This can greatly speed up the initial wallet "
                  << "scanning process."
                  << std::endl
                  << "If you do not know the exact height, "
                  << std::endl
                  << "err on the side of caution so transactions do not "
                  << "get missed."
                  << std::endl
                  << "Hit enter for the default of zero: ";

        std::string stringHeight;

        std::getline(std::cin, stringHeight);

        /* Remove commas so user can enter height as e.g. 200,000 */
        boost::erase_all(stringHeight, ",");

        if (stringHeight == "")
        {
            return 0;
        }

        try
        {
            return std::stoi(stringHeight);
        }
        catch (const std::invalid_argument &)
        {
            std::cout << WarningMsg("Failed to parse height - input is not ")
                      << WarningMsg("a number!") << std::endl << std::endl;
        }
    }
}

void viewWalletMsg()
{
    std::cout << InformationMsg("Please remember that when using a view wallet "
                                "you can only view incoming transactions!")
              << std::endl << "This means if you received 100 "
              << WalletConfig::ticker << " and then "
              << "sent 50 " << WalletConfig::ticker << ", your balance "
              << "would appear to still be 100 "
              << WalletConfig::ticker << "." << std::endl
              << "To effectively use a view wallet, you should only deposit "
              << "to this wallet." << std::endl
              << "If you have since needed to withdraw, send your remaining "
              << "balance to a new wallet,"
              << std::endl
              << "and import this as a new view "
              << "wallet so your balance can be correctly observed."
              << std::endl << std::endl;
}

void connectingMsg()
{
    std::cout << std::endl << "Making initial contact with "
              << WalletConfig::daemonName
              << "."
              << std::endl
              << "Please wait, this sometimes can take a long time..."
              << std::endl << std::endl;
}

void promptSaveKeys(CryptoNote::WalletGreen &wallet)
{
    std::cout << "Welcome to your new wallet, here is your payment address:"
              << std::endl << InformationMsg(wallet.getAddress(0))
              << std::endl << std::endl 
              << "Please copy your secret keys and mnemonic seed and store "
              << "them in a secure location: " << std::endl;

    printPrivateKeys(wallet, false);

    std::cout << std::endl;
}
