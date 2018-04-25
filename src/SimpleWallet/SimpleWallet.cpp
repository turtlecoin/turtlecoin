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

#include <SimpleWallet/SimpleWallet.h>

int main(int argc, char **argv)
{
    /* On ctrl+c the program seems to throw "simplewallet.exe has stopped
       working" when calling exit(0)... I'm not sure why, this is a bit of
       a hack, it disables that */
    #ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    #endif

    Config config = parseArguments(argc, argv);

    /* User requested --help or --version, or invalid arguments */
    if (config.exit)
    {
        return 0;
    }

    /* Only log to file so we don't have crap filling up the terminal */
    Logging::LoggerManager logManager;
    /* Should we maybe change this to INFO? On the one hand, DEBUGGING really
       spams the file, on the other hand, it exposes a lot of information
       that might help more devs debug a users issues. */
    logManager.setMaxLevel(Logging::DEBUGGING);

    Logging::FileLogger fileLogger;
    fileLogger.init("simplewallet.log");
    logManager.addLogger(fileLogger);

    Logging::LoggerRef logger(logManager, "simplewallet");

    /* Currency contains our coin parameters, such as decimal places, supply */
    CryptoNote::Currency currency 
        = CryptoNote::CurrencyBuilder(logManager).currency();

    System::Dispatcher localDispatcher;
    System::Dispatcher *dispatcher = &localDispatcher;

    /* Our connection to turtlecoind */
    std::unique_ptr<CryptoNote::INode> node(
        new CryptoNote::NodeRpcProxy(config.host, config.port, 
                                     logger.getLogger()));

    std::promise<std::error_code> errorPromise;
    std::future<std::error_code> error = errorPromise.get_future();
    auto callback = [&errorPromise](std::error_code e) 
                    {errorPromise.set_value(e); };

    node->init(callback);

    if (error.get())
    {
        throw std::runtime_error("Failed to initialize node!");
    }

    /* Create the wallet instance */
    CryptoNote::WalletGreen wallet(*dispatcher, currency, *node, 
                                   logger.getLogger());

    /* Run the interactive wallet interface */
    run(wallet, *node, config);
}

void run(CryptoNote::WalletGreen &wallet, CryptoNote::INode &node,
         Config &config)
{
    auto maybeWalletInfo = Nothing<std::shared_ptr<WalletInfo>>();
    Action action;

    do
    {
        std::cout << InformationMsg("TurtleCoin v"
                                  + std::string(PROJECT_VERSION)
                                  + " Simplewallet") << std::endl;

        /* Open/import/generate the wallet */
        action = getAction(config);
        maybeWalletInfo = handleAction(wallet, action, config);

    /* Didn't manage to get the wallet info, returning to selection screen */
    } while (!maybeWalletInfo.isJust);

    auto walletInfo = maybeWalletInfo.x;

    bool alreadyShuttingDown = false;

    /* This will call shutdown when ctrl+c is hit. This is a lambda function,
       & means capture all variables by reference */
    Tools::SignalHandler::install([&] {
        /* If we're already shutting down let control flow continue as normal */
        if (shutdown(walletInfo->wallet, node, alreadyShuttingDown))
        {
            exit(0);
        }
    });

    while (node.getLastKnownBlockHeight() == 0)
    {
        std::cout << WarningMsg("It looks like TurtleCoind isn't open!")
                  << std::endl << std::endl
                  << WarningMsg("Ensure TurtleCoind is open and has finished "
                                "initializing.")
                  << std::endl
                  << WarningMsg("If it's still not working, try restarting "
                                "TurtleCoind. The daemon sometimes gets stuck.") 
                  << std::endl
                  << WarningMsg("Alternatively, perhaps TurtleCoind can't "
                                "communicate with any peers.")
                  << std::endl << std::endl
                  << WarningMsg("The wallet can't function until it can "
                                "communicate with the network.")
                  << std::endl << std::endl;

        bool proceed = false;

        while (true)
        {
            std::cout << "[" << InformationMsg("T") << "]ry again, "
                      << "[" << InformationMsg("E") << "]xit, or "
                      << "[" << InformationMsg("C") << "]ontinue anyway?: ";

            std::string answer;
            std::getline(std::cin, answer);

            char c = std::tolower(answer[0]);

            /* Lets people spam enter in the transaction screen */
            if (c == 't' || c == '\0')
            {
                break;
            }
            else if (c == 'e' || c == std::ifstream::traits_type::eof())
            {
                shutdown(walletInfo->wallet, node, alreadyShuttingDown);
                return;
            }
            else if (c == 'c')
            {
                proceed = true;
                break;
            }
            else
            {
                std::cout << WarningMsg("Bad input: ") << InformationMsg(answer)
                          << WarningMsg(" - please enter either T, E, or C.")
                          << std::endl;
            }
        }

        if (proceed)
        {
            break;
        }

        std::cout << std::endl;
    }

    /* Scan the chain for new transactions. In the case of an imported 
       wallet, we need to scan the whole chain to find any transactions. 
       If we opened the wallet however, we just need to scan from when we 
       last had it open. If we are generating a wallet, there is no need
       to check for transactions as there is no way the wallet can have
       received any money yet. */
    if (action != Generate)
    {
        findNewTransactions(node, walletInfo);

    }
    else
    {
        std::cout << InformationMsg("Your wallet is syncing with the "
                                    "network in the background.")
                  << std::endl
                  << InformationMsg("Until this is completed new "
                                    "transactions might not show up.")
                  << std::endl
                  << InformationMsg("Use bc_height to check the progress.")
                  << std::endl << std::endl;
    }

    welcomeMsg();

    inputLoop(walletInfo, node);

    shutdown(walletInfo->wallet, node, alreadyShuttingDown);
}

Maybe<std::shared_ptr<WalletInfo>> handleAction(CryptoNote::WalletGreen &wallet,
                                                Action action, Config &config)
{
    if (action == Generate)
    {
        return Just<std::shared_ptr<WalletInfo>>(generateWallet(wallet));
    }
    else if (action == Open)
    {
        return openWallet(wallet, config);
    }
    else if (action == Import)
    {
        return Just<std::shared_ptr<WalletInfo>>(importWallet(wallet));
    }
    else if (action == SeedImport)
    {
        return Just<std::shared_ptr<WalletInfo>>(mnemonicImportWallet(wallet));
    }
    else if (action == ViewWallet)
    {
        return Just<std::shared_ptr<WalletInfo>>(createViewWallet(wallet));
    }
    else
    {
        throw std::runtime_error("Unimplemented action!");
    }
}

std::shared_ptr<WalletInfo> createViewWallet(CryptoNote::WalletGreen &wallet)
{
    Crypto::SecretKey privateViewKey = getPrivateKey("Private View Key: ");

    CryptoNote::AccountPublicAddress publicKeys;
    uint64_t prefix;

    std::string address;

    while (true)
    {
        std::cout << "Public TRTL address: ";

        std::getline(std::cin, address);
        boost::algorithm::trim(address);

        if (address.length() != 99)
        {
            std::cout << WarningMsg("Address is wrong length!") << std::endl
                      << "It should be 99 characters long, but it is "
                      << address.length() << " characters long!" << std::endl;
        }
        else if (address.substr(0, 4) != "TRTL")
        {
            std::cout << WarningMsg("Invalid address! It should start with "
                                    "\"TRTL\"!") << std::endl;
        }
        else if (!CryptoNote::parseAccountAddressString(prefix, publicKeys,
                                                        address))
        {
            std::cout << WarningMsg("Failed to parse TRTL address! Ensure you "
                                    "have entered it correctly.")
                      << std::endl;
        }
        else
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
    }
    while (!crypto::ElectrumWords::is_valid_mnemonic(mnemonicPhrase,
                                                     privateSpendKey));

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
        }
        else
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
            }
            else
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
        catch (const std::system_error& e)
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
                                        "don't have simplewallet open twice.")
                          << std::endl
                          << WarningMsg("Also check you don't have another "
                                        "wallet program open, such as a GUI "
                                        "wallet or walletd.")
                          << std::endl << std::endl;

                std::cout << "Returning to selection screen..." << std::endl
                          << std::endl;

                return Nothing<std::shared_ptr<WalletInfo>>();
            }
            else if (errorMsg == notAWalletMsg)
            {
                std::cout << WarningMsg("Could not open wallet file! It "
                                        "doesn't appear to be a valid wallet!")
                          << std::endl
                          << WarningMsg("Ensure you are opening a wallet "
                                        "file, and the file has not gotten "
                                        "corrupted.")
                          << std::endl
                          << WarningMsg("Try reimporting via keys, and always "
                                        "close simplewallet with the exit "
                                        "command to prevent corruption.")
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
        }
        else if (boost::filesystem::exists(walletFileName))
        {
            return walletFileName;
        }
        else
        {
            std::cout << WarningMsg("A wallet with the filename " 
                                  + walletFileName + " doesn't exist!")
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

std::string getWalletPassword(bool verifyPwd)
{
    Tools::PasswordContainer pwdContainer;
    pwdContainer.read_password(verifyPwd);
    return pwdContainer.password();
}

Action getAction(Config &config)
{
    if (config.walletGiven || config.passGiven)
    {
        return Open;
    }

    while (true)
    {
        std::cout << std::endl << "Welcome, please choose an option below:"
                  << std::endl << std::endl
                  
                  << "\t[" << InformationMsg("G") << "] - "
                  << "Generate a new wallet address"
                  << std::endl 

                  << "\t[" << InformationMsg("O") << "] - "
                  << "Open a wallet already on your system"
                  << std::endl
                  
                  << "\t[" << InformationMsg("S") << "] - "
                  << "Regenerate your wallet using a seed phrase of words"
                  << std::endl
                  
                  << "\t[" << InformationMsg("I") << "] - "
                  << "Import your wallet using a View Key and Spend Key"
                  << std::endl

                  << "\t[" << InformationMsg("V") << "] - "
                  << "Import a view only wallet (Unable to send transactions)"
                  << std::endl << std::endl

                  << "or, press CTRL_C to exit: ";

        std::string answer;
        std::getline(std::cin, answer);

        char c = answer[0];
        c = std::tolower(c);

        if (c == 'o')
        {
            return Open;
        }
        else if (c == 'g')
        {
            return Generate;
        }
        else if (c == 'i')
        {
            return Import;
        }
        else if (c == 's')
        {
            return SeedImport;
        }
        else if (c == 'v')
        {
            return ViewWallet;
        }
        else
        {
            std::cout << "Unknown command: " << WarningMsg(answer) << std::endl;
        }
    }
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

void exportKeys(std::shared_ptr<WalletInfo> &walletInfo)
{
    confirmPassword(walletInfo->walletPass);
    printPrivateKeys(walletInfo->wallet, walletInfo->viewWallet);
}

void printPrivateKeys(CryptoNote::WalletGreen &wallet, bool viewWallet)
{
    Crypto::SecretKey privateViewKey = wallet.getViewKey().secretKey;

    if (viewWallet)
    {
        std::cout << SuccessMsg("Private view key: " 
                              + Common::podToHex(privateViewKey)) << std::endl;
        return;
    }

    Crypto::SecretKey privateSpendKey = wallet.getAddressSpendKey(0).secretKey;

    Crypto::SecretKey derivedPrivateViewKey;

    CryptoNote::AccountBase::generateViewFromSpend(privateSpendKey,
                                                   derivedPrivateViewKey);

    bool deterministicPrivateKeys = derivedPrivateViewKey == privateViewKey;

    std::cout << SuccessMsg("Private spend key: " 
                          + Common::podToHex(privateSpendKey)) << std::endl
              << SuccessMsg("Private view key: " 
                          + Common::podToHex(privateViewKey)) << std::endl;

    if (deterministicPrivateKeys)
    {
        std::string mnemonicSeed;

        crypto::ElectrumWords::bytes_to_words(privateSpendKey, 
                                              mnemonicSeed,
                                              "English");

        std::cout << SuccessMsg("Mnemonic seed: " + mnemonicSeed) << std::endl;
    }
}

void welcomeMsg()
{
    std::cout << "Use the " << SuggestionMsg("help") 
              << " command to see the list "
              << "of available commands." << std::endl << "Use "
              << SuggestionMsg("exit") << " when closing to ensure your wallet "
              << "file doesn't get corrupted." << std::endl << std::endl;
}

std::string getInputAndDoWorkWhileIdle(std::shared_ptr<WalletInfo> &walletInfo)
{
    auto lastUpdated = std::chrono::system_clock::now();

    std::future<std::string> inputGetter = std::async(std::launch::async, [] {
            std::string command;
            std::getline(std::cin, command);
            boost::algorithm::trim(command);
            return command;
    });

    while (true)
    {
        /* Check if the user has inputted something yet (Wait for zero seconds
           to instantly return) */
        std::future_status status = inputGetter.wait_for(std::chrono::seconds(0));

        /* User has inputted, get what they inputted and return it */
        if (status == std::future_status::ready)
        {
            return inputGetter.get();
        }

        auto currentTime = std::chrono::system_clock::now();

        /* Otherwise check if we need to update the wallet cache */
        if ((currentTime - lastUpdated) > std::chrono::seconds(5))
        {
            lastUpdated = currentTime;
            checkForNewTransactions(walletInfo);
        }

        /* Sleep for enough for it to not be noticeable when the user enters
           something, but enough that we're not starving the CPU */
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void inputLoop(std::shared_ptr<WalletInfo> &walletInfo, CryptoNote::INode &node)
{ 
    while (true)
    {
        std::cout << getPrompt(walletInfo);

        std::string command = getInputAndDoWorkWhileIdle(walletInfo);

        /* Split into args to support legacy transfer command, for example
           transfer 5 TRTLxyz... 100, sends 100 TRTL to TRTLxyz... with a mixin
           of 5 */
        std::vector<std::string> words;
        words = boost::split(words, command, ::isspace);

        if (command == "")
        {
            // no-op
        }
        else if (command == "export_keys")
        {
            exportKeys(walletInfo);
        }
        else if (command == "help")
        {
            help(walletInfo->viewWallet);
        }
        else if (command == "balance")
        {
            balance(node, walletInfo->wallet, walletInfo->viewWallet);
        }
        else if (command == "address")
        {
            std::cout << SuccessMsg(walletInfo->walletAddress) << std::endl;
        }
        else if (command == "incoming_transfers")
        {
            listTransfers(true, false, walletInfo->wallet, node);
        }
        else if (command == "exit")
        {
            return;
        }
        else if (command == "save")
        {
            std::cout << InformationMsg("Saving.") << std::endl;
            walletInfo->wallet.save();
            std::cout << InformationMsg("Saved.") << std::endl;
        }
        else if (command == "bc_height")
        {
            blockchainHeight(node, walletInfo->wallet);
        }
        else if (command == "reset")
        {
            reset(node, walletInfo);
        }
        else if (!walletInfo->viewWallet)
        {
            if (command == "outgoing_transfers")
            {
                listTransfers(false, true, walletInfo->wallet, node);
            }
            else if (command == "list_transfers")
            {
                listTransfers(true, true, walletInfo->wallet, node);
            }
            else if (command == "transfer")
            {
                transfer(walletInfo);
            }
            else if (words[0] == "transfer")
            {
                /* remove the first item from words - this is the "transfer"
                   command, leaving us just the transfer arguments. */
                words.erase(words.begin());
                transfer(walletInfo, words);
            }
            else if (command == "quick_optimize")
            {
                quickOptimize(walletInfo->wallet);
            }
            else if (command == "full_optimize")
            {
                fullOptimize(walletInfo->wallet);
            }
            else
            {
                std::cout << "Unknown command: " << WarningMsg(command) 
                          << ", use " << SuggestionMsg("help") 
                          << " command to list all possible commands."
                          << std::endl;
            }
        }
        else
        {
            std::cout << "Unknown command: " << WarningMsg(command) 
                      << ", use " << SuggestionMsg("help") 
                      << " command to list all possible commands." << std::endl
                      << "Please note some commands such as transfer are "
                      << "unavailable, as you are using a view only wallet."
                      << std::endl;
        }
    }
}

void help(bool viewWallet)
{
    std::cout << "Available commands:" << std::endl
              << SuccessMsg("help", 25)
              << "List this help message" << std::endl
              << SuccessMsg("reset", 25)
              << "Discard cached data and recheck for transactions" << std::endl
              << SuccessMsg("bc_height", 25)
              << "Show the blockchain height" << std::endl
              << SuccessMsg("balance", 25)
              << "Display how much TRTL you have" << std::endl
              << SuccessMsg("export_keys", 25)
              << "Export your private keys" << std::endl
              << SuccessMsg("address", 25)
              << "Displays your payment address" << std::endl
              << SuccessMsg("exit", 25)
              << "Exit and save your wallet" << std::endl
              << SuccessMsg("save", 25)
              << "Save your wallet state" << std::endl
              << SuccessMsg("incoming_transfers", 25)
              << "Show incoming transfers" << std::endl;
                  
    if (viewWallet)
    {
        std::cout << InformationMsg("Please note you are using a view only "
                                    "wallet, and so cannot transfer TRTL.")
                  << std::endl;
    }
    else
    {
        std::cout << SuccessMsg("outgoing_transfers", 25)
                  << "Show outgoing transfers" << std::endl
                  << SuccessMsg("list_transfers", 25)
                  << "Show all transfers" << std::endl
                  << SuccessMsg("quick_optimize", 25)
                  << "Quickly optimize your wallet to send large amounts"
                  << std::endl
                  << SuccessMsg("full_optimize", 25)
                  << "Fully optimize your wallet to send large amounts"
                  << std::endl
                  << SuccessMsg("transfer", 25)
                  << "Send TRTL to someone" << std::endl;
    }
}

void balance(CryptoNote::INode &node, CryptoNote::WalletGreen &wallet,
             bool viewWallet)
{
    uint64_t unconfirmedBalance = wallet.getPendingBalance();
    uint64_t confirmedBalance = wallet.getActualBalance();
    uint64_t totalBalance = unconfirmedBalance + confirmedBalance;

    uint32_t localHeight = node.getLastLocalBlockHeight();
    uint32_t remoteHeight = node.getLastKnownBlockHeight();
    uint32_t walletHeight = wallet.getBlockCount();

    std::cout << "Available balance: "
              << SuccessMsg(formatAmount(confirmedBalance)) << std::endl
              << "Locked (unconfirmed) balance: "
              << WarningMsg(formatAmount(unconfirmedBalance))
              << std::endl << "Total balance: "
              << InformationMsg(formatAmount(totalBalance)) << std::endl;

    if (viewWallet)
    {
        std::cout << std::endl 
                  << InformationMsg("Please note that view only wallets "
                                    "can only track incoming transactions, "
                                    "and so your wallet balance may appear "
                                    "inflated.") << std::endl;
    }

    if (localHeight < remoteHeight)
    {
        std::cout << std::endl
                  << InformationMsg("Your daemon is not fully synced with "
                                    "the network!")
                  << std::endl << "Your balance may be incorrect until you "
                  << "are fully synced!" << std::endl;
    }
    /* Small buffer because wallet height doesn't update instantly like node
       height does */
    else if (walletHeight + 1000 < remoteHeight)
    {
        std::cout << std::endl
                  << InformationMsg("The blockchain is still being scanned for "
                                    "your transactions.")
                  << std::endl
                  << "Balances might be incorrect whilst this is ongoing."
                  << std::endl;
    }
}

void blockchainHeight(CryptoNote::INode &node, CryptoNote::WalletGreen &wallet)
{
    uint32_t localHeight = node.getLastLocalBlockHeight();
    uint32_t remoteHeight = node.getLastKnownBlockHeight();
    uint32_t walletHeight = wallet.getBlockCount();

    /* This is the height that the wallet has been scanned to. The blockchain
       can be fully updated, but we have to walk the chain to find our
       transactions, and this number indicates that progress. */
    std::cout << "Wallet blockchain height: ";

    /* Small buffer because wallet height doesn't update instantly like node
       height does */
    if (walletHeight + 1000 > remoteHeight)
    {
        std::cout << SuccessMsg(std::to_string(walletHeight));
    }
    else
    {
        std::cout << WarningMsg(std::to_string(walletHeight));
    }

    std::cout << std::endl << "Local blockchain height: ";

    if (localHeight == remoteHeight)
    {
        std::cout << SuccessMsg(std::to_string(localHeight));
    }
    else
    {
        std::cout << WarningMsg(std::to_string(localHeight));
    }

    std::cout << std::endl << "Network blockchain height: "
              << SuccessMsg(std::to_string(remoteHeight)) << std::endl;

    if (localHeight == 0 && remoteHeight == 0)
    {
        std::cout << WarningMsg("Uh oh, it looks like you don't have "
                                "TurtleCoind open!")
                  << std::endl;
    }
    else if (walletHeight + 1000 < remoteHeight && localHeight == remoteHeight)
    {
        std::cout << InformationMsg("You are synced with the network, but the "
                                    "blockchain is still being scanned for "
                                    "your transactions.")
                  << std::endl
                  << "Balances might be incorrect whilst this is ongoing."
                  << std::endl;
    }
    else if (localHeight == remoteHeight)
    {
        std::cout << SuccessMsg("Yay! You are synced!") << std::endl;
    }
    else
    {
        std::cout << WarningMsg("Be patient, you are still syncing with the "
                                "network!") << std::endl;
    }
}

bool shutdown(CryptoNote::WalletGreen &wallet, CryptoNote::INode &node,
              bool &alreadyShuttingDown)
{
    if (alreadyShuttingDown)
    {
        std::cout << "Patience little turtle, we're already shutting down!" 
                  << std::endl;
        return false;
    }
    else
    {
        alreadyShuttingDown = true;
        std::cout << InformationMsg("Saving wallet and shutting down, please "
                                    "wait...") << std::endl;
    }

    bool finishedShutdown = false;

    boost::thread timelyShutdown([&finishedShutdown]
    {
        auto startTime = std::chrono::system_clock::now();

        /* Has shutdown finished? */
        while (!finishedShutdown)
        {
            auto currentTime = std::chrono::system_clock::now();

            /* If not, wait for a max of 20 seconds then force exit. */
            if ((currentTime - startTime) > std::chrono::seconds(20))
            {
                std::cout << WarningMsg("Wallet took too long to save! "
                                        "Force closing.") << std::endl
                          << "Bye." << std::endl;
                exit(0);
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    wallet.save();
    wallet.shutdown();
    node.shutdown();

    finishedShutdown = true;

    /* Wait for shutdown watcher to finish */
    timelyShutdown.join();

    std::cout << "Bye." << std::endl;

    return true;
}

CryptoNote::BlockDetails getBlock(uint32_t blockHeight,
                                  CryptoNote::INode &node)
{
    CryptoNote::BlockDetails block;

    /* No connection to turtlecoind */
    if (node.getLastKnownBlockHeight() == 0)
    {
        return block;
    }

    std::promise<std::error_code> errorPromise;

    auto e = errorPromise.get_future();

    auto callback = [&errorPromise](std::error_code e)
    {
        errorPromise.set_value(e);
    };

    node.getBlock(blockHeight, block, callback);

    if (e.get())
    {
        /* Prevent the compiler optimizing it out... */
        std::cout << "";
    }

    return block;
}

std::string getBlockTime(CryptoNote::BlockDetails b)
{
    if (b.timestamp == 0)
    {
        return "";
    }

    std::time_t time = b.timestamp;
    char buffer[100];
    std::strftime(buffer, sizeof(buffer), "%F %R", std::localtime(&time));
    return std::string(buffer);
}

void printOutgoingTransfer(CryptoNote::WalletTransaction t,
                           CryptoNote::INode &node)
{
    std::string blockTime = getBlockTime(getBlock(t.blockHeight, node));

    std::cout << WarningMsg("Outgoing transfer: " + Common::podToHex(t.hash))
              << std::endl
              << WarningMsg("Spent: " + formatAmount(-t.totalAmount - t.fee))
              << std::endl
              << WarningMsg("Fee: " + formatAmount(t.fee))
              << std::endl
              << WarningMsg("Total Spent: " + formatAmount(-t.totalAmount))
              << std::endl;

    /* Couldn't get timestamp, maybe old node or turtlecoind closed */
    if (blockTime != "")
    {
        std::cout << WarningMsg("Timestamp: " + blockTime) << std::endl;
    }
    
    std::cout << std::endl;
}

void printIncomingTransfer(CryptoNote::WalletTransaction t,
                           CryptoNote::INode &node)
{
    std::string blockTime = getBlockTime(getBlock(t.blockHeight, node));

    std::cout << SuccessMsg("Incoming transfer: " + Common::podToHex(t.hash))
              << std::endl
              << SuccessMsg("Amount: " + formatAmount(t.totalAmount))
              << std::endl;

    /* Couldn't get timestamp, maybe old node or turtlecoind closed */
    if (blockTime != "")
    {
        std::cout << SuccessMsg("Timestamp: " + blockTime) << std::endl;
    }
    
    std::cout << std::endl;
}

void listTransfers(bool incoming, bool outgoing, 
                   CryptoNote::WalletGreen &wallet, CryptoNote::INode &node)
{
    size_t numTransactions = wallet.getTransactionCount();
    int64_t totalSpent = 0;
    int64_t totalReceived = 0;

    for (size_t i = 0; i < numTransactions; i++)
    {
        CryptoNote::WalletTransaction t = wallet.getTransaction(i);

        if (t.totalAmount < 0 && outgoing)
        {
            printOutgoingTransfer(t, node);
            totalSpent += -t.totalAmount;
        }
        else if (t.totalAmount > 0 && incoming)
        {
            printIncomingTransfer(t, node);
            totalReceived += t.totalAmount;
        }
    }

    if (incoming)
    {
        std::cout << SuccessMsg("Total received: " 
                              + formatAmount(totalReceived))
                  << std::endl;
    }

    if (outgoing)
    {
        std::cout << WarningMsg("Total spent: " + formatAmount(totalSpent))
                  << std::endl;
    }
}

void checkForNewTransactions(std::shared_ptr<WalletInfo> &walletInfo)
{
    walletInfo->wallet.updateInternalCache();

    size_t newTransactionCount = walletInfo->wallet.getTransactionCount();

    if (newTransactionCount != walletInfo->knownTransactionCount)
    {
        for (size_t i = walletInfo->knownTransactionCount; 
                    i < newTransactionCount; i++)
        {
            CryptoNote::WalletTransaction t 
                = walletInfo->wallet.getTransaction(i);

            /* Don't print outgoing or fusion transfers */
            if (t.totalAmount > 0)
            {
                std::cout << std::endl
                          << InformationMsg("New transaction found!")
                          << std::endl
                          << SuccessMsg("Incoming transfer: " 
                                      + Common::podToHex(t.hash))
                          << std::endl
                          << SuccessMsg("Amount: "
                                      + formatAmount(t.totalAmount))
                          << std::endl
                          << getPrompt(walletInfo)
                          << std::flush;
            }
        }

        walletInfo->knownTransactionCount = newTransactionCount;
    }
}

void reset(CryptoNote::INode &node, std::shared_ptr<WalletInfo> &walletInfo)
{
    std::cout << InformationMsg("Resetting wallet...") << std::endl;

    walletInfo->knownTransactionCount = 0;

    /* Wallet is now unitialized. You must reinit with load, initWithKeys,
       or whatever. This function wipes the cache, then saves the wallet. */
    walletInfo->wallet.clearCacheAndShutdown();

    /* Now, we reopen the wallet. It now has no cached tx's, and balance */
    walletInfo->wallet.load(walletInfo->walletFileName,
                            walletInfo->walletPass);

    /* Now we rescan the chain to re-discover our balance and transactions */
    findNewTransactions(node, walletInfo);
}

void findNewTransactions(CryptoNote::INode &node, 
                         std::shared_ptr<WalletInfo> &walletInfo)
{
    uint32_t localHeight = node.getLastLocalBlockHeight();
    uint32_t walletHeight = walletInfo->wallet.getBlockCount();
    uint32_t remoteHeight = node.getLastKnownBlockHeight();

    size_t transactionCount = walletInfo->wallet.getTransactionCount();

    int stuckCounter = 0;

    if (localHeight != remoteHeight)
    {
        std::cout << "Your TurtleCoind isn't fully synced yet!" << std::endl
                  << "Until you are fully synced, you won't be able to send "
                  << "transactions, and your balance may be missing or "
                  << "incorrect!" << std::endl << std::endl;
    }

    /* If we open a legacy wallet then it will load the transactions but not
       have the walletHeight == transaction height. Lets just throw away the
       transactions and rescan. */
    if (walletHeight == 1 && transactionCount != 0)
    {
        std::cout << "Upgrading your wallet from an older version of the "
                  << "software..." << std::endl << "Unfortunately, we have "
                  << "to rescan the chain to find your transactions."
                  << std::endl;
        transactionCount = 0;
        walletInfo->wallet.clearCaches(true, false);
    }

    if (walletHeight == 1)
    {
        std::cout << "Scanning through the blockchain to find transactions "
                     "that belong to you." << std::endl
                     << "Please wait, this will take some time."
                     << std::endl << std::endl;
    }
    else
    {
        std::cout << "Scanning through the blockchain to find any new "
                     "transactions you received whilst your wallet wasn't "
                     "open." << std::endl
                     << "Please wait, this may take some time."
                     << std::endl << std::endl;
    }

    while (walletHeight < localHeight)
    {
        int counter = 1;

        /* This MUST be called on the main thread! */
        walletInfo->wallet.updateInternalCache();

        localHeight = node.getLastLocalBlockHeight();
        remoteHeight = node.getLastKnownBlockHeight();
        std::cout << SuccessMsg(std::to_string(walletHeight))
                  << " of " << InformationMsg(std::to_string(localHeight))
                  << std::endl;

        uint32_t tmpWalletHeight = walletInfo->wallet.getBlockCount();

        int waitSeconds = 1;

        /* Save periodically so if someone closes before completion they don't
           lose all their progress */
        if (counter % 60 == 0)
        {
            walletInfo->wallet.save();
        }

        if (tmpWalletHeight == walletHeight)
        {
            stuckCounter++;
            waitSeconds = 3;

            if (stuckCounter > 20)
            {
                std::string warning =
                    "Syncing may be stuck. Try restarting Turtlecoind.\n"
                    "If this persists, visit "
                    "https://turtlecoin.lol/#contact for support.";
                std::cout << WarningMsg(warning) << std::endl;
            }
            else if (stuckCounter > 19)
            {
                /*
                   Calling save has the side-effect of starting
                   and stopping blockchainSynchronizer, which seems
                   to sometimes force the sync to resume properly.
                   So we'll try this before warning the user.
                */
                std::cout << InformationMsg("Saving wallet.") << std::endl;
                walletInfo->wallet.save();
                waitSeconds = 5;
            }
        }
        else
        {
            stuckCounter = 0;
            walletHeight = tmpWalletHeight;

            size_t tmpTransactionCount = walletInfo
                                       ->wallet.getTransactionCount();

            if (tmpTransactionCount != transactionCount)
            {
                for (size_t i = transactionCount; i < tmpTransactionCount; i++)
                {
                    CryptoNote::WalletTransaction t
                        = walletInfo->wallet.getTransaction(i);

                    /* Don't print out fusion transactions */
                    if (t.totalAmount != 0)
                    {
                        std::cout << std::endl
                                  << InformationMsg("New transaction found!")
                                  << std::endl << std::endl;

                        if (t.totalAmount < 0)
                        {
                            printOutgoingTransfer(t, node);
                        }
                        else
                        {
                            printIncomingTransfer(t, node);
                        }
                    }
                }

                transactionCount = tmpTransactionCount;
            }
        }

        counter++;

        std::this_thread::sleep_for(std::chrono::seconds(waitSeconds));
    }

    std::cout << SuccessMsg("Finished scanning blockchain!") << std::endl
              << std::endl;

    /* In case the user force closes, we don't want them to have to rescan
       the whole chain. */
    walletInfo->wallet.save();

    walletInfo->knownTransactionCount = transactionCount;
}

ColouredMsg getPrompt(std::shared_ptr<WalletInfo> &walletInfo)
{
    const int promptLength = 20;
    const std::string extension = ".wallet";

    std::string walletName = walletInfo->walletFileName;

    /* Filename ends in .wallet, remove extension */
    if (std::equal(extension.rbegin(), extension.rend(), 
                   walletInfo->walletFileName.rbegin()))
    {
        size_t extPos = walletInfo->walletFileName.find_last_of('.');

        walletName = walletInfo->walletFileName.substr(0, extPos);
    }

    std::string shortName = walletName.substr(0, promptLength);

    return InformationMsg("[TRTL " + shortName + "]: ");
}

void connectingMsg()
{
    std::cout << std::endl << "Making initial contact with TurtleCoind."
              << std::endl
              << "Please wait, this sometimes can take a long time..."
              << std::endl << std::endl;
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
