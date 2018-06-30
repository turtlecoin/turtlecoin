/*
Copyright (C) 2018, The TurtleCoin developers

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

//////////////////////////////
#include <ZedWallet/Open.h>
//////////////////////////////

#include <boost/algorithm/string.hpp>

#include <CryptoNoteCore/Account.h>
#include <CryptoNoteCore/CryptoNoteBasicImpl.h>

#include <Mnemonics/electrum-words.h>

#include <ZedWallet/ColouredMsg.h>
#include <ZedWallet/Commands.h>
#include <ZedWallet/Transfer.h>
#include <ZedWallet/Types.h>
#include <ZedWallet/PasswordContainer.h>

std::shared_ptr<WalletInfo> createViewWallet(CryptoNote::WalletGreen &wallet)
{
    Crypto::SecretKey privateViewKey = getPrivateKey("Private View Key: ");

    std::string address;

    while (true)
    {
        std::cout << "Public TRTL address: ";

        std::getline(std::cin, address);
        boost::algorithm::trim(address);

        if (parseAddress(address))
        {
            break;
        }
    }

    std::string walletFileName = getNewWalletFileName();
    std::string walletPass = getWalletPassword(true);

    wallet.createViewWallet(walletFileName, walletPass, address,
                            privateViewKey);

    std::cout << InformationMsg("\nYour view wallet " + address
                                + " has been successfully imported!")
              << std::endl << std::endl;

    viewWalletMsg();

    return std::make_shared<WalletInfo>(walletFileName, walletPass,
                                        address, true, wallet);
}

std::shared_ptr<WalletInfo> importWallet(CryptoNote::WalletGreen &wallet)
{
    Crypto::SecretKey privateSpendKey = getPrivateKey("Private Spend Key: ");
    Crypto::SecretKey privateViewKey = getPrivateKey("Private View Key: ");
    return importFromKeys(wallet, privateSpendKey, privateViewKey);
}

std::shared_ptr<WalletInfo> mnemonicImportWallet(CryptoNote::WalletGreen
                                                 &wallet)
{
    std::string mnemonicPhrase;

    Crypto::SecretKey privateSpendKey;
    Crypto::SecretKey privateViewKey;

    do
    {
        std::cout << "Mnemonic Phrase (25 words): ";
        std::getline(std::cin, mnemonicPhrase);
        boost::algorithm::trim(mnemonicPhrase);
    } while (!crypto::ElectrumWords::is_valid_mnemonic(mnemonicPhrase,
                                                       privateSpendKey,
                                                       std::cout));

    CryptoNote::AccountBase::generateViewFromSpend(privateSpendKey,
                                                   privateViewKey);

    return importFromKeys(wallet, privateSpendKey, privateViewKey);
}

std::shared_ptr<WalletInfo> importFromKeys(CryptoNote::WalletGreen &wallet,
                                           Crypto::SecretKey privateSpendKey,
                                           Crypto::SecretKey privateViewKey)
{
    std::string walletFileName = getNewWalletFileName();
    std::string walletPass = getWalletPassword(true);

    connectingMsg();

    wallet.initializeWithViewKey(walletFileName, walletPass, privateViewKey);

    std::string walletAddress = wallet.createAddress(privateSpendKey);

    std::cout << InformationMsg("\nYour wallet " + walletAddress
                                + " has been successfully imported!")
              << std::endl << std::endl;

    return std::make_shared<WalletInfo>(walletFileName, walletPass,
                                        walletAddress, false, wallet);
}

std::shared_ptr<WalletInfo> generateWallet(CryptoNote::WalletGreen &wallet)
{
    std::string walletFileName = getNewWalletFileName();
    std::string walletPass = getWalletPassword(true);


    CryptoNote::KeyPair spendKey;
    Crypto::SecretKey privateViewKey;

    Crypto::generate_keys(spendKey.publicKey, spendKey.secretKey);
    CryptoNote::AccountBase::generateViewFromSpend(spendKey.secretKey,
                                                   privateViewKey);

    wallet.initializeWithViewKey(walletFileName, walletPass, privateViewKey);

    std::string walletAddress = wallet.createAddress(spendKey.secretKey);

    promptSaveKeys(wallet);

    std::cout << WarningMsg("If you lose these your wallet cannot be recreated!")
              << std::endl << std::endl;

    return std::make_shared<WalletInfo>(walletFileName, walletPass,
                                        walletAddress, false, wallet);
}

Maybe<std::shared_ptr<WalletInfo>> openWallet(CryptoNote::WalletGreen &wallet,
                                              Config &config)
{
    std::string walletFileName = getExistingWalletFileName(config);

    bool initial = true;

    while (true)
    {
        std::string walletPass;

        /* Only use the command line pass once, otherwise we will infinite
           loop if it is incorrect */
        if (initial && config.passGiven)
        {
            walletPass = config.walletPass;
        } else
        {
            walletPass = getWalletPassword(false);
        }

        initial = false;

        connectingMsg();

        try
        {
            wallet.load(walletFileName, walletPass);

            std::string walletAddress = wallet.getAddress(0);

            Crypto::SecretKey privateSpendKey
                    = wallet.getAddressSpendKey(0).secretKey;

            if (privateSpendKey == CryptoNote::NULL_SECRET_KEY)
            {
                std::cout << std::endl
                          << InformationMsg("Your view only wallet "
                                            + walletAddress
                                            + " has been successfully opened!")
                          << std::endl << std::endl;

                viewWalletMsg();

                return Just<std::shared_ptr<WalletInfo>>
                        (std::make_shared<WalletInfo>(walletFileName,
                                                      walletPass,
                                                      walletAddress,
                                                      true,
                                                      wallet));
            } else
            {
                std::cout << std::endl
                          << InformationMsg("Your wallet "
                                            + walletAddress
                                            + " has been successfully opened!")
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
        catch (const std::system_error &e)
        {
            std::string walletSuccessBadPwdMsg =
                    "Restored view public key doesn't correspond to secret key: "
                    "The password is wrong";

            std::string walletSuccessBadPwdMsg2 =
                    "Restored spend public key doesn't correspond to secret key: "
                    "The password is wrong";

            std::string walletLegacyBadPwdMsg =
                    ": The password is wrong";

            std::string alreadyOpenMsg =
                    "MemoryMappedFile::open: The process cannot access the file "
                    "because it is being used by another process.";

            std::string notAWalletMsg =
                    "Unsupported wallet version: Wrong version";

            std::string notAWalletMsg2 =
                    "Failed to read wallet version: Wrong version";

            std::string errorMsg = e.what();

            /* There are three different error messages depending upon if we're
               opening a walletgreen or a walletlegacy wallet */
            if (errorMsg == walletSuccessBadPwdMsg ||
                errorMsg == walletSuccessBadPwdMsg2 ||
                errorMsg == walletLegacyBadPwdMsg)
            {
                std::cout << WarningMsg("Incorrect password! Try again.")
                          << std::endl;
            }
                /* The message actually has a \r\n on the end but i'd prefer to
                   keep just the raw string in the source so check the it starts
                   with instead */
            else if (boost::starts_with(errorMsg, alreadyOpenMsg))
            {
                std::cout << WarningMsg("Could not open wallet! It is already "
                                        "open in another process.")
                          << std::endl
                          << WarningMsg("Check with a task manager that you "
                                        "don't have zedwallet open twice.")
                          << std::endl
                          << WarningMsg("Also check you don't have another "
                                        "wallet program open, such as a GUI "
                                        "wallet or walletd.")
                          << std::endl << std::endl;

                std::cout << "Returning to selection screen..." << std::endl
                          << std::endl;

                return Nothing<std::shared_ptr<WalletInfo>>();
            } else if (errorMsg == notAWalletMsg || errorMsg == notAWalletMsg2)
            {
                std::cout << WarningMsg("Could not open wallet file! It "
                                        "doesn't appear to be a valid wallet!")
                          << std::endl
                          << WarningMsg("Ensure you are opening a wallet "
                                        "file, and the file has not gotten "
                                        "corrupted.")
                          << std::endl
                          << WarningMsg("Try reimporting via keys, and always "
                                        "close zedwallet with the exit "
                                        "command to prevent corruption.")
                          << std::endl << std::endl;

                std::cout << "Returning to selection screen..." << std::endl
                          << std::endl;

                return Nothing<std::shared_ptr<WalletInfo>>();
            } else
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
    size_t privateKeyLen = 64;
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
        } else if (!Common::fromHex(privateKeyString, &privateKeyHash,
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
        } else
        {
            std::cout << "What is the name of the wallet you want to open?: ";
            std::getline(std::cin, walletName);
        }

        initial = false;

        std::string walletFileName = walletName + ".wallet";

        if (walletName == "")
        {
            std::cout << WarningMsg("Wallet name can't be blank! Try again.")
                      << std::endl;
        }
            /* Allow people to enter wallet name with or without file extension */
        else if (boost::filesystem::exists(walletName))
        {
            return walletName;
        } else if (boost::filesystem::exists(walletFileName))
        {
            return walletFileName;
        } else
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

        std::string walletFileName = walletName + ".wallet";

        if (boost::filesystem::exists(walletFileName))
        {
            std::cout << WarningMsg("A wallet with the filename "
                                    + walletFileName + " already exists!")
                      << std::endl
                      << "Try another name." << std::endl;
        } else if (walletName == "")
        {
            std::cout << WarningMsg("Wallet name can't be blank! Try again.")
                      << std::endl;
        } else
        {
            return walletFileName;
        }
    }
}

std::string getWalletPassword(bool verifyPwd)
{
    Tools::PasswordContainer pwdContainer;
    pwdContainer.read_password(verifyPwd);
    return pwdContainer.password();
}

void viewWalletMsg()
{
    std::cout << InformationMsg("Please remember that when using a view wallet "
                                "you can only view incoming transactions!")
              << std::endl << "This means if you received 100 TRTL and then "
              << "sent 50 TRTL, your balance would appear to still be 100 "
              << "TRTL." << std::endl
              << "To effectively use a view wallet, you should only deposit "
              << "to this wallet." << std::endl
              << "If you have since needed to withdraw, send your remaining "
              << "balance to a new wallet, and import this as a new view "
              << "wallet so your balance can be correctly observed."
              << std::endl << std::endl;
}

void connectingMsg()
{
    std::cout << std::endl << "Making initial contact with TurtleCoind."
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
