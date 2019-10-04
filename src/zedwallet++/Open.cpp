// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

/////////////////////////////
#include <zedwallet++/Open.h>
/////////////////////////////

#include <common/FileSystemShim.h>
#include <config/WalletConfig.h>
#include <errors/ValidateParameters.h>
#include <iostream>
#include <mnemonics/Mnemonics.h>
#include <utilities/ColouredMsg.h>
#include <utilities/Input.h>
#include <utilities/String.h>
#include <zedwallet++/CommandImplementations.h>
#include <utilities/PasswordContainer.h>
#include <zedwallet++/Utilities.h>

std::shared_ptr<WalletBackend> importViewWallet(const ZedConfig &config)
{
    std::cout << WarningMsg("View wallets are only for viewing incoming ")
              << WarningMsg("transactions, and cannot make transfers.") << std::endl;

    bool create = Utilities::confirm("Is this OK?");

    std::cout << "\n";

    if (!create)
    {
        return nullptr;
    }

    Crypto::SecretKey privateViewKey = getPrivateKey("Private View Key: ");

    std::string address;

    while (true)
    {
        std::cout << InformationMsg("Enter your public ") << InformationMsg(WalletConfig::ticker)
                  << InformationMsg(" address: ");

        std::getline(std::cin, address);

        Utilities::trim(address);

        const bool integratedAddressesAllowed = false;

        if (Error error = validateAddresses({address}, integratedAddressesAllowed); error != SUCCESS)
        {
            std::cout << WarningMsg("Invalid address: ") << WarningMsg(error) << std::endl;
        }
        else
        {
            break;
        }
    }

    const std::string walletFileName = getNewWalletFileName();

    const std::string msg = "Give your new wallet a password: ";

    const bool verifyPassword = true;

    const std::string walletPass = getWalletPassword(verifyPassword, msg);

    const uint64_t scanHeight = ZedUtilities::getScanHeight();

    auto [error, walletBackend] = WalletBackend::importViewWallet(
        privateViewKey,
        address,
        walletFileName,
        walletPass,
        scanHeight,
        config.host,
        config.port,
        config.ssl,
        config.threads);

    if (error)
    {
        std::cout << WarningMsg("Failed to import wallet: " + error.getErrorMessage());

        return nullptr;
    }

    std::stringstream stream;

    stream << "\nYour view only wallet " << walletBackend->getPrimaryAddress() << " has been successfully imported!\n";

    std::cout << InformationMsg(stream.str()) << std::endl;

    viewWalletMsg();

    return walletBackend;
}

std::shared_ptr<WalletBackend> importWalletFromKeys(const ZedConfig &config)
{
    const Crypto::SecretKey privateSpendKey = getPrivateKey("Enter your private spend key: ");

    const Crypto::SecretKey privateViewKey = getPrivateKey("Enter your private view key: ");

    const std::string walletFileName = getNewWalletFileName();

    const std::string msg = "Give your new wallet a password: ";

    const bool verifyPassword = true;

    const std::string walletPass = getWalletPassword(verifyPassword, msg);

    const uint64_t scanHeight = ZedUtilities::getScanHeight();

    const auto [error, walletBackend] = WalletBackend::importWalletFromKeys(
        privateSpendKey,
        privateViewKey,
        walletFileName,
        walletPass,
        scanHeight,
        config.host,
        config.port,
        config.ssl,
        config.threads);

    if (error)
    {
        std::cout << WarningMsg("Failed to import wallet: " + error.getErrorMessage());

        return nullptr;
    }

    std::stringstream stream;

    stream << "\nYour wallet " << walletBackend->getPrimaryAddress() << " has been successfully imported!\n";

    std::cout << InformationMsg(stream.str()) << std::endl;

    return walletBackend;
}

std::shared_ptr<WalletBackend> importWalletFromSeed(const ZedConfig &config)
{
    std::string mnemonicSeed;

    while (true)
    {
        std::cout << InformationMsg("Enter your mnemonic phrase (25 words): ");

        std::getline(std::cin, mnemonicSeed);

        Utilities::trim(mnemonicSeed);

        /* Just to check if it's valid */
        auto [error, privateSpendKey] = Mnemonics::MnemonicToPrivateKey(mnemonicSeed);

        if (!error)
        {
            break;
        }

        std::cout << std::endl << WarningMsg(error.getErrorMessage()) << std::endl << std::endl;
    }

    const std::string walletFileName = getNewWalletFileName();

    const std::string msg = "Give your new wallet a password: ";

    const bool verifyPassword = true;

    const std::string walletPass = getWalletPassword(verifyPassword, msg);

    const uint64_t scanHeight = ZedUtilities::getScanHeight();

    auto [error, walletBackend] = WalletBackend::importWalletFromSeed(
        mnemonicSeed, walletFileName, walletPass, scanHeight, config.host, config.port, config.ssl, config.threads);

    if (error)
    {
        std::cout << WarningMsg("Failed to import wallet: " + error.getErrorMessage());

        return nullptr;
    }

    std::stringstream stream;

    stream << "\nYour wallet " << walletBackend->getPrimaryAddress() << " has been successfully imported!\n";

    std::cout << InformationMsg(stream.str()) << std::endl;

    return walletBackend;
}

std::shared_ptr<WalletBackend> createWallet(const ZedConfig &config)
{
    const std::string walletFileName = getNewWalletFileName();

    const std::string msg = "Give your new wallet a password: ";

    const bool verifyPassword = true;

    const std::string walletPass = getWalletPassword(verifyPassword, msg);

    const auto [error, walletBackend] =
        WalletBackend::createWallet(walletFileName, walletPass, config.host, config.port, config.ssl, config.threads);

    if (error)
    {
        std::cout << WarningMsg("Failed to create wallet: " + error.getErrorMessage()) << std::endl;

        return nullptr;
    }

    std::cout << "\n";

    promptSaveKeys(walletBackend);

    std::cout << WarningMsg("If you lose these your wallet cannot be ") << WarningMsg("recreated!") << std::endl
              << std::endl;

    return walletBackend;
}

std::shared_ptr<WalletBackend> openWallet(const ZedConfig &config)
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
            const bool verifyPassword = false;

            walletPass = getWalletPassword(verifyPassword, "Enter password: ");
        }

        const auto [error, walletBackend] =
            WalletBackend::openWallet(walletFileName, walletPass, config.host, config.port, config.ssl, config.threads);

        if (error == WRONG_PASSWORD)
        {
            /* Don't reuse cli password */
            initial = false;

            std::cout << std::endl << WarningMsg("Incorrect password! Try again.") << std::endl << std::endl;

            continue;
        }
        else if (error)
        {
            std::cout << WarningMsg("Failed to open wallet: " + error.getErrorMessage()) << std::endl;

            return nullptr;
        }

        std::stringstream stream;

        stream << "\nYour wallet " << walletBackend->getPrimaryAddress() << " has been successfully opened!\n";

        std::cout << InformationMsg(stream.str()) << std::endl;

        return walletBackend;
    }
}

Crypto::SecretKey getPrivateKey(const std::string outputMsg)
{
    const uint64_t privateKeyLen = 64;
    uint64_t size;

    std::string privateKeyString;
    Crypto::Hash privateKeyHash;
    Crypto::SecretKey privateKey;
    Crypto::PublicKey publicKey;

    while (true)
    {
        std::cout << InformationMsg(outputMsg);

        std::getline(std::cin, privateKeyString);

        Utilities::trim(privateKeyString);

        if (privateKeyString.length() != privateKeyLen)
        {
            std::cout << std::endl
                      << WarningMsg("Invalid private key, should be 64 ") << WarningMsg("characters! Try again.")
                      << std::endl
                      << std::endl;

            continue;
        }
        else if (
            !Common::fromHex(privateKeyString, &privateKeyHash, sizeof(privateKeyHash), size)
            || size != sizeof(privateKeyHash))
        {
            std::cout << WarningMsg("Invalid private key, it is not a valid ") << WarningMsg("hex string! Try again.")
                      << std::endl
                      << std::endl;

            continue;
        }

        privateKey = *(struct Crypto::SecretKey *)&privateKeyHash;

        /* Just used for verification */
        if (!Crypto::secret_key_to_public_key(privateKey, publicKey))
        {
            std::cout << std::endl
                      << WarningMsg("Invalid private key, is not on the ") << WarningMsg("ed25519 curve!") << std::endl
                      << WarningMsg("Probably a typo - ensure you entered ") << WarningMsg("it correctly.") << std::endl
                      << std::endl;

            continue;
        }

        return privateKey;
    }
}

std::string getExistingWalletFileName(const ZedConfig &config)
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
            std::cout << InformationMsg("What is the name of the wallet ") << InformationMsg("you want to open?: ");

            std::getline(std::cin, walletName);
        }

        initial = false;

        const std::string walletFileName = walletName + ".wallet";

        try
        {
            if (walletName == "")
            {
                std::cout << WarningMsg("\nWallet name can't be blank! Try again.\n\n");
            }
            /* Allow people to enter wallet name with or without file extension */
            else if (fs::exists(walletName) && !fs::is_directory(walletName))
            {
                return walletName;
            }
            else if (fs::exists(walletFileName) && !fs::is_directory(walletFileName))
            {
                return walletFileName;
            }
            else
            {
                std::cout << WarningMsg("\nA wallet with the filename ") << InformationMsg(walletName)
                          << WarningMsg(" or ") << InformationMsg(walletFileName) << WarningMsg(" doesn't exist!\n")
                          << "Ensure you entered your wallet name correctly.\n\n";
            }
        }
        catch (const fs::filesystem_error &)
        {
            std::cout << WarningMsg("\nInvalid wallet filename! Try again.\n\n");
        }
    }
}

std::string getNewWalletFileName()
{
    std::string walletName;

    while (true)
    {
        std::cout << InformationMsg("What would you like to call your ") << InformationMsg("new wallet?: ");

        std::getline(std::cin, walletName);

        const std::string walletFileName = walletName + ".wallet";

        try
        {
            if (fs::exists(walletFileName))
            {
                std::cout << std::endl
                          << WarningMsg("A wallet with the filename ") << InformationMsg(walletFileName)
                          << WarningMsg(" already exists!") << std::endl
                          << "Try another name." << std::endl
                          << std::endl;
            }
            else if (walletName == "")
            {
                std::cout << std::endl
                          << WarningMsg("Wallet name can't be blank! Try again.") << std::endl
                          << std::endl;
            }
            else
            {
                return walletFileName;
            }
        }
        catch (const fs::filesystem_error &)
        {
            std::cout << WarningMsg("\nInvalid wallet filename! Try again.\n\n");
        }
    }
}

std::string getWalletPassword(const bool verifyPwd, const std::string msg)
{
    Tools::PasswordContainer pwdContainer;
    pwdContainer.read_password(verifyPwd, msg);
    return pwdContainer.password();
}

void viewWalletMsg()
{
    std::cout << InformationMsg("Please remember that when using a view wallet "
                                "you can only view incoming transactions!")
              << std::endl
              << InformationMsg("Therefore, if you have recieved transactions ")
              << InformationMsg("which you then spent, your balance will ") << InformationMsg("appear inflated.")
              << std::endl;
}

void promptSaveKeys(const std::shared_ptr<WalletBackend> walletBackend)
{
    std::cout << "Welcome to your new wallet, here is your payment address:"
              << "\n"
              << InformationMsg(walletBackend->getPrimaryAddress())
              << "\n\nPlease copy your secret keys and mnemonic seed and store "
              << "them in a secure location:\n\n";

    printPrivateKeys(walletBackend);

    std::cout << std::endl;
}
