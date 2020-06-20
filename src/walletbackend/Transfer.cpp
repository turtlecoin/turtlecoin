// Copyright (c) 2018-2020, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

///////////////////////////////////
#include <walletbackend/Transfer.h>
///////////////////////////////////

#include <config/Constants.h>
#include <config/WalletConfig.h>
#include <common/Varint.h>
#include <errors/ValidateParameters.h>
#include <logger/Logger.h>
#include <utilities/Addresses.h>
#include <utilities/FormatTools.h>
#include <utilities/Mixins.h>
#include <utilities/Utilities.h>
#include <walletbackend/WalletBackend.h>

namespace SendTransaction
{
    std::tuple<Error, Crypto::Hash>
        sendFusionTransactionBasic(const std::shared_ptr<Nigel> daemon, const std::shared_ptr<SubWallets> subWallets)
    {
        const auto [minMixin, maxMixin, defaultMixin] = Utilities::getMixinAllowableRange(daemon->networkBlockCount());

        /* Assumes the container has at least one subwallet - this is true as long
           as the static constructors were used */
        const std::string defaultAddress = subWallets->getPrimaryAddress();

        return sendFusionTransactionAdvanced(defaultMixin, {}, defaultAddress, daemon, subWallets, {}, std::nullopt);
    }

    std::tuple<Error, Crypto::Hash> sendFusionTransactionAdvanced(
        const uint64_t mixin,
        const std::vector<std::string> addressesToTakeFrom,
        std::string destination,
        const std::shared_ptr<Nigel> daemon,
        const std::shared_ptr<SubWallets> subWallets,
        const std::vector<uint8_t> extraData,
        const std::optional<uint64_t> optimizeTarget)
    {
        if (destination == "")
        {
            destination = subWallets->getPrimaryAddress();
        }

        /* Validate the transaction input parameters */
        Error error = validateFusionTransaction(
            mixin,
            addressesToTakeFrom,
            destination,
            subWallets,
            daemon->networkBlockCount(),
            optimizeTarget
        );

        if (error)
        {
            return {error, Crypto::Hash()};
        }

        /* If no address to take from is given, we will take from all available. */
        const bool takeFromAllSubWallets = addressesToTakeFrom.empty();

        /* Convert the addresses to public spend keys */
        const std::vector<Crypto::PublicKey> subWalletsToTakeFrom =
            Utilities::addressesToSpendKeys(addressesToTakeFrom);

        /* Grab inputs for our fusion transaction */
        auto [ourInputs, maxFusionInputs, foundMoney] = subWallets->getFusionTransactionInputs(
            takeFromAllSubWallets,
            subWalletsToTakeFrom,
            mixin,
            daemon->networkBlockCount(),
            optimizeTarget
        );

        /* Mixin is too large to get enough outputs whilst remaining in the size
           and ratio constraints */
        if (maxFusionInputs < CryptoNote::parameters::FUSION_TX_MIN_INPUT_COUNT)
        {
            return {FUSION_MIXIN_TOO_LARGE, Crypto::Hash()};
        }

        /* Payment ID's are not needed with fusion transactions */
        const std::string paymentID = "";

        CryptoNote::Transaction tx;

        std::vector<WalletTypes::KeyOutput> transactionOutputs;

        CryptoNote::KeyPair txKeyPair;

        while (true)
        {
            /* Not got enough unspent inputs for a fusion tx - we're fully optimized. */
            if (ourInputs.size() < CryptoNote::parameters::FUSION_TX_MIN_INPUT_COUNT)
            {
                return {FULLY_OPTIMIZED, Crypto::Hash()};
            }

            /* Grab the public keys from the receiver address */
            const auto [publicSpendKey, publicViewKey] = Utilities::addressToKeys(destination);

            std::vector<WalletTypes::TransactionDestination> destinations;

            uint64_t amountToSplit = foundMoney;

            /* We have an optimize target, and enough money to make outputs
             * large enough for the target, lets attempt to make some outputs
             * of that size. */
            /* Disabled for now as this will cause the wallet to not create valid
             * fusion transactions as fusions are required to decompose as
             * efficiently as possible - i.e. 12345 -> 10000 + 2000 + 300 + 40 + 5 */
            /* if (optimizeTarget && foundMoney >= *optimizeTarget) */
            if (false)
            {
                /* Max amount of optimizeTarget destinations we can make */
                const uint64_t numTargets = foundMoney / *optimizeTarget;

                /* Change remaining after making as many optimizeTarget destinations */
                amountToSplit = foundMoney % *optimizeTarget;

                WalletTypes::TransactionDestination destination;

                destination.amount = *optimizeTarget;
                destination.receiverPublicSpendKey = publicSpendKey;
                destination.receiverPublicViewKey = publicViewKey;

                /* Insert all optimizeTarget destination amounts */
                destinations.insert(destinations.end(), numTargets, destination);
            }

            /* Split transfer into denominations and create an output for each */
            for (const auto denomination : splitAmountIntoDenominations(amountToSplit))
            {
                WalletTypes::TransactionDestination destination;

                destination.amount = denomination;
                destination.receiverPublicSpendKey = publicSpendKey;
                destination.receiverPublicViewKey = publicViewKey;

                destinations.push_back(destination);
            }

            uint64_t requiredInputOutputRatio = CryptoNote::parameters::FUSION_TX_MIN_IN_OUT_COUNT_RATIO;

            /* We need to have at least 4x more inputs than outputs */
            if (destinations.size() == 0 || (ourInputs.size() / destinations.size()) < requiredInputOutputRatio)
            {
                /* Reduce the amount we're sending */
                foundMoney -= ourInputs.back().input.amount;

                /* Remove the last input */
                ourInputs.pop_back();

                /* And try again */
                continue;
            }

            const uint64_t unlockTime = 0;

            WalletTypes::TransactionResult txResult =
                makeTransaction(mixin, daemon, ourInputs, paymentID, destinations, subWallets, unlockTime, extraData);

            tx = txResult.transaction;
            transactionOutputs = txResult.outputs;
            txKeyPair = txResult.txKeyPair;

            if (txResult.error)
            {
                return {txResult.error, Crypto::Hash()};
            }

            const uint64_t txSize = toBinaryArray(tx).size();

            /* Transaction is too large, remove an input and try again */
            if (txSize > CryptoNote::parameters::FUSION_TX_MAX_SIZE)
            {
                /* Reduce the amount we're sending */
                foundMoney -= ourInputs.back().input.amount;

                /* Remove the last input */
                ourInputs.pop_back();

                /* And try again */
                continue;
            }

            break;
        }

        if (!verifyAmounts(tx))
        {
            return {AMOUNTS_NOT_PRETTY, Crypto::Hash()};
        }

        const uint64_t actualFee = sumTransactionFee(tx);

        if (!verifyTransactionFee(WalletTypes::FeeType::FixedFee(0), actualFee, tx))
        {
            return {UNEXPECTED_FEE, Crypto::Hash()};
        }

        /* Try and send the transaction */
        const auto [sendError, txHash] = relayTransaction(tx, daemon);

        if (sendError)
        {
            return {sendError, Crypto::Hash()};
        }

        /* No fee or change with fusion */
        const uint64_t fee(0), changeRequired(0);

        /* Store the unconfirmed transaction, update our balance */
        storeSentTransaction(txHash, fee, paymentID, ourInputs, destination, changeRequired, subWallets);

        /* Update our locked balance with the incoming funds */
        storeUnconfirmedIncomingInputs(subWallets, transactionOutputs, txKeyPair.publicKey, txHash);

        subWallets->storeTxPrivateKey(txKeyPair.secretKey, txHash);

        /* Lock the input for spending till it is confirmed as spent in a block */
        for (const auto &input : ourInputs)
        {
            subWallets->markInputAsLocked(input.input.keyImage, input.publicSpendKey);
        }

        return {SUCCESS, txHash};
    }

    /* A basic send transaction, the most common transaction, one destination,
       default fee, default mixin, default change address

       WARNING: This is NOT suitable for multi wallet containers, as the change
       will be returned to the primary subwallet address.

       If you want to return change to a specific wallet, use
       sendTransactionAdvanced() */
    std::tuple<Error, Crypto::Hash, WalletTypes::PreparedTransactionInfo> sendTransactionBasic(
        std::string destination,
        const uint64_t amount,
        std::string paymentID,
        const std::shared_ptr<Nigel> daemon,
        const std::shared_ptr<SubWallets> subWallets,
        const bool sendAll,
        const bool sendTransaction)
    {
        std::vector<std::pair<std::string, uint64_t>> destinations = {{destination, amount}};

        const auto [minMixin, maxMixin, defaultMixin] = Utilities::getMixinAllowableRange(daemon->networkBlockCount());

        WalletTypes::FeeType fee = WalletTypes::FeeType::MinimumFee();

        /* Assumes the container has at least one subwallet - this is true as long
           as the static constructors were used */
        const std::string changeAddress = subWallets->getPrimaryAddress();

        const uint64_t unlockTime = 0;

        return sendTransactionAdvanced(
            destinations,
            defaultMixin,
            fee,
            paymentID,
            {},
            changeAddress,
            daemon,
            subWallets,
            unlockTime,
            {},
            sendAll,
            sendTransaction
        );
    }

    std::tuple<Error, Crypto::Hash, WalletTypes::PreparedTransactionInfo> sendTransactionAdvanced(
        std::vector<std::pair<std::string, uint64_t>> addressesAndAmounts,
        const uint64_t mixin,
        const WalletTypes::FeeType fee,
        std::string paymentID,
        const std::vector<std::string> addressesToTakeFrom,
        std::string changeAddress,
        const std::shared_ptr<Nigel> daemon,
        const std::shared_ptr<SubWallets> subWallets,
        const uint64_t unlockTime,
        const std::vector<uint8_t> extraData,
        const bool sendAll,
        const bool sendTransaction)
    {
        /* Append the fee transaction, if a fee is being used */
        const auto [feeAmount, feeAddress] = daemon->nodeFee();

        if (feeAmount != 0)
        {
            addressesAndAmounts.push_back({feeAddress, feeAmount});
        }

        if (changeAddress == "")
        {
            changeAddress = subWallets->getPrimaryAddress();
        }

        /* Validate the transaction input parameters */
        Error error = validateTransaction(
            addressesAndAmounts,
            mixin,
            fee,
            paymentID,
            addressesToTakeFrom,
            changeAddress,
            subWallets,
            daemon->networkBlockCount());

        if (error)
        {
            return {error, Crypto::Hash(), WalletTypes::PreparedTransactionInfo()};
        }

        /* Convert integrated addresses to standard address + paymentID, if
           present. We have already validated they are valid integrated addresses
           in validateTransaction(), and the paymentID's do not conflict. */
        for (auto &[address, amount] : addressesAndAmounts)
        {
            if (address.length() != WalletConfig::integratedAddressLength)
            {
                continue;
            }

            auto [extractedAddress, extractedPaymentID] = Utilities::extractIntegratedAddressData(address);

            address = extractedAddress;
            paymentID = extractedPaymentID;
        }

        /* If no address to take from is given, we will take from all available. */
        const bool takeFromAllSubWallets = addressesToTakeFrom.empty();

        /* The total amount we are sending */
        uint64_t totalAmount = Utilities::getTransactionSum(addressesAndAmounts);

        /* Convert the addresses to public spend keys */
        const std::vector<Crypto::PublicKey> subWalletsToTakeFrom =
            Utilities::addressesToSpendKeys(addressesToTakeFrom);

        /* Get inputs that are available to be spent so we can form the tx */
        auto availableInputs = subWallets->getSpendableTransactionInputs(
            takeFromAllSubWallets,
            subWalletsToTakeFrom,
            daemon->networkBlockCount()
        );

        uint64_t sumOfInputs = 0;

        std::vector<WalletTypes::TxInputAndOwner> ourInputs;

        if (fee.isFixedFee)
        {
            totalAmount += fee.fixedFee;
        }

        WalletTypes::TransactionResult txResult;
        uint64_t changeRequired;
        uint64_t requiredAmount = totalAmount;
        WalletTypes::PreparedTransactionInfo txInfo;

        for (const auto &input : availableInputs)
        {
            ourInputs.push_back(input);
            sumOfInputs += input.input.amount;

            if (sumOfInputs >= totalAmount)
            {
                /* If the sum of inputs is > total amount, we need to send some back to
                   ourselves. */
                changeRequired = sumOfInputs - totalAmount;

                /* Split the transfers up into an amount, a public spend+view key */
                auto destinations = setupDestinations(addressesAndAmounts, changeRequired, changeAddress);

                /* Ok, we are using a fee per byte, lets take a guess at how
                 * large our fee is going to be, and then see if we have enough
                 * inputs to cover it. */
                if (!fee.isFixedFee)
                {
                    const size_t transactionSize = Utilities::estimateTransactionSize(
                        mixin,
                        ourInputs.size(),
                        destinations.size(),
                        paymentID != "",
                        extraData.size()
                    );

                    const double feePerByte = fee.isFeePerByte
                        ? fee.feePerByte
                        : CryptoNote::parameters::MINIMUM_FEE_PER_BYTE_V1;

                    const uint64_t estimatedFee = Utilities::getTransactionFee(
                        transactionSize,
                        daemon->networkBlockCount(),
                        feePerByte
                    );

                    if (sendAll)
                    {
                        const auto [address, amount] = addressesAndAmounts[0];

                        if (estimatedFee > amount)
                        {
                            txInfo.fee = estimatedFee;
                            return { NOT_ENOUGH_BALANCE, Crypto::Hash(), txInfo };
                        }

                        totalAmount -= estimatedFee;
                        addressesAndAmounts[0] = { address, amount - estimatedFee };
                        destinations = setupDestinations(addressesAndAmounts, changeRequired, changeAddress);
                    }

                    const uint64_t estimatedAmount = totalAmount + estimatedFee;

                    /* Ok, we have enough inputs to add our estimated fee, lets
                     * go ahead and try and make the transaction. */
                    if (sumOfInputs >= estimatedAmount)
                    {
                        const auto [success, result, change, needed] = tryMakeFeePerByteTransaction(
                            sumOfInputs,
                            totalAmount,
                            estimatedAmount,
                            feePerByte,
                            addressesAndAmounts,
                            changeAddress,
                            mixin,
                            daemon,
                            ourInputs,
                            paymentID,
                            subWallets,
                            unlockTime,
                            extraData,
                            sendAll
                        );

                        if (success)
                        {
                            txResult = result;
                            changeRequired = change;
                            break;
                        }
                        else
                        {
                            requiredAmount = needed;
                            continue;
                        }
                    }
                    else
                    {
                        /* Need to ensure we update this so if we run out of
                         * inputs we correctly check if we have enough balance */
                        requiredAmount = estimatedAmount;
                    }
                }
                else
                {
                    txResult = makeTransaction(
                        mixin,
                        daemon,
                        ourInputs,
                        paymentID,
                        destinations,
                        subWallets,
                        unlockTime,
                        extraData
                    );

                    const uint64_t minFee = Utilities::getMinimumTransactionFee(
                        toBinaryArray(txResult.transaction).size(),
                        daemon->networkBlockCount()
                    );

                    if (fee.fixedFee >= minFee)
                    {
                        break;
                    }
                    else
                    {
                        return { FEE_TOO_SMALL, Crypto::Hash(), WalletTypes::PreparedTransactionInfo() };
                    }
                }
            }
        }

        if (sumOfInputs < requiredAmount)
        {
            txInfo.fee = requiredAmount - totalAmount;
            return {NOT_ENOUGH_BALANCE, Crypto::Hash(), txInfo};
        }

        if (txResult.error)
        {
            return {txResult.error, Crypto::Hash(), txInfo};
        }

        error = isTransactionPayloadTooBig(txResult.transaction, daemon->networkBlockCount());

        if (error)
        {
            return {error, Crypto::Hash(), txInfo};
        }

        if (!verifyAmounts(txResult.transaction))
        {
            return {AMOUNTS_NOT_PRETTY, Crypto::Hash(), txInfo};
        }

        const uint64_t actualFee = sumTransactionFee(txResult.transaction);

        if (!verifyTransactionFee(fee, actualFee, txResult.transaction))
        {
            return {UNEXPECTED_FEE, Crypto::Hash(), txInfo};
        }

        txInfo.fee = actualFee;
        txInfo.paymentID = paymentID;
        txInfo.inputs = ourInputs;
        txInfo.changeAddress = changeAddress;
        txInfo.changeRequired = changeRequired;
        txInfo.tx = txResult;

        if (sendTransaction)
        {
            const auto [sendError, txHash] = relayTransaction(txResult.transaction, daemon);

            if (sendError)
            {
                return {sendError, Crypto::Hash(), WalletTypes::PreparedTransactionInfo()};
            }

            txInfo.transactionHash = txHash;

            /* Store the unconfirmed transaction, update our balance */
            storeSentTransaction(txHash, actualFee, paymentID, ourInputs, changeAddress, changeRequired, subWallets);

            /* Update our locked balance with the incoming funds */
            storeUnconfirmedIncomingInputs(subWallets, txResult.outputs, txResult.txKeyPair.publicKey, txHash);

            subWallets->storeTxPrivateKey(txResult.txKeyPair.secretKey, txHash);

            /* Lock the input for spending till it is confirmed as spent in a block */
            for (const auto &input : ourInputs)
            {
                subWallets->markInputAsLocked(input.input.keyImage, input.publicSpendKey);
            }

            return {SUCCESS, txHash, txInfo};
        }
        else
        {
            txInfo.transactionHash = getTransactionHash(txResult.transaction);
            return {SUCCESS, txInfo.transactionHash, txInfo};
        }
    }

    std::tuple<Error, Crypto::Hash> sendPreparedTransaction(
        const WalletTypes::PreparedTransactionInfo txInfo,
        const std::shared_ptr<Nigel> daemon,
        const std::shared_ptr<SubWallets> subWallets)
    {
        for (const auto &input : txInfo.inputs)
        {
            if (!subWallets->haveSpendableInput(input.input, daemon->networkBlockCount()))
            {
                return {PREPARED_TRANSACTION_EXPIRED, Crypto::Hash()};
            }
        }

        const auto [sendError, txHash] = relayTransaction(txInfo.tx.transaction, daemon);

        if (sendError)
        {
            return {sendError, Crypto::Hash()};
        }

        /* Store the unconfirmed transaction, update our balance */
        storeSentTransaction(
            txHash,
            txInfo.fee,
            txInfo.paymentID,
            txInfo.inputs,
            txInfo.changeAddress,
            txInfo.changeRequired,
            subWallets
        );

        /* Update our locked balance with the incoming funds */
        storeUnconfirmedIncomingInputs(
            subWallets,
            txInfo.tx.outputs,
            txInfo.tx.txKeyPair.publicKey,
            txHash
        );

        subWallets->storeTxPrivateKey(txInfo.tx.txKeyPair.secretKey, txHash);

        /* Lock the input for spending till it is confirmed as spent in a block */
        for (const auto &input : txInfo.inputs)
        {
            subWallets->markInputAsLocked(input.input.keyImage, input.publicSpendKey);
        }

        return {SUCCESS, txHash};
    }

    std::tuple<bool, WalletTypes::TransactionResult, uint64_t, uint64_t> tryMakeFeePerByteTransaction(
        const uint64_t sumOfInputs,
        uint64_t amountPreFee,
        uint64_t amountIncludingFee,
        const double feePerByte,
        std::vector<std::pair<std::string, uint64_t>> addressesAndAmounts,
        const std::string changeAddress,
        const uint64_t mixin,
        const std::shared_ptr<Nigel> daemon,
        const std::vector<WalletTypes::TxInputAndOwner> ourInputs,
        const std::string paymentID,
        const std::shared_ptr<SubWallets> subWallets,
        const uint64_t unlockTime,
        const std::vector<uint8_t> extraData,
        const bool sendAll)
    {
        while (true)
        {
            const uint64_t changeRequired = sumOfInputs - amountIncludingFee;

            /* Need to recalculate destinations since amount of change, err, changed! */
            const auto destinations = setupDestinations(addressesAndAmounts, changeRequired, changeAddress);

            WalletTypes::TransactionResult txResult = makeTransaction(
                mixin,
                daemon,
                ourInputs,
                paymentID,
                destinations,
                subWallets,
                unlockTime,
                extraData
            );

            const size_t actualTxSize = toBinaryArray(txResult.transaction).size();

            const uint64_t actualFee = Utilities::getTransactionFee(
                actualTxSize,
                daemon->networkBlockCount(),
                feePerByte
            );

            /* Great! The fee we estimated is greater than or equal
             * to the min/specified fee per byte for a transaction
             * of this size, so we can continue with sending the
             * transaction. */
            if (amountIncludingFee - amountPreFee >= actualFee)
            {
                return { true, txResult, changeRequired, 0 };
            }

            /* If we're sending all, then we adjust the amount we're sending,
             * rather than the change we're returning. */
            if (sendAll)
            {
                amountPreFee = amountIncludingFee - actualFee;
                const auto [address, amount] = addressesAndAmounts[0];
                addressesAndAmounts[0] = { address, amountPreFee };
            }

            /* The actual fee required for a tx of this size is not
             * covered by the amount of inputs we have so far, lets
             * go select some more then try again. */
            if (amountPreFee + actualFee > sumOfInputs)
            {
                return { false, txResult, changeRequired, amountPreFee + actualFee };
            }

            /* Our fee was too low. Lets try making the transaction again,
             * this time using the actual fee calculated. Note that this still
             * may fail, since we are possibly adding more outputs, and so have
             * a large transaction size. If we keep increasing the fee and keep
             * failing, eventually we'll hit a point where we either succeed
             * or we need to gather more inputs. */
            amountIncludingFee = amountPreFee + actualFee;
        }

        throw std::runtime_error("Programmer error @ tryMakeFeePerByteTransaction");
    }

    Error isTransactionPayloadTooBig(const CryptoNote::Transaction tx, const uint64_t currentHeight)
    {
        const uint64_t txSize = toBinaryArray(tx).size();

        const uint64_t maxTxSize = Utilities::getMaxTxSize(currentHeight);

        if (txSize > maxTxSize)
        {
            std::stringstream errorMsg;

            errorMsg << "Transaction is too large: (" << Utilities::prettyPrintBytes(txSize)
                     << "). Max allowed size is " << Utilities::prettyPrintBytes(maxTxSize)
                     << ". Decrease the amount you are sending, or perform some "
                     << "fusion transactions.";

            return Error(TOO_MANY_INPUTS_TO_FIT_IN_BLOCK, errorMsg.str());
        }

        return SUCCESS;
    }

    /* Possibly we could abstract some of this from processTransactionOutputs...
       but I think it would make the code harder to follow */
    void storeUnconfirmedIncomingInputs(
        const std::shared_ptr<SubWallets> subWallets,
        const std::vector<WalletTypes::KeyOutput> keyOutputs,
        const Crypto::PublicKey txPublicKey,
        const Crypto::Hash txHash)
    {
        Crypto::KeyDerivation derivation;

        Crypto::generate_key_derivation(txPublicKey, subWallets->getPrivateViewKey(), derivation);

        uint64_t outputIndex = 0;

        for (const auto output : keyOutputs)
        {
            Crypto::PublicKey spendKey;

            /* Not our output */
            Crypto::underive_public_key(derivation, outputIndex, output.key, spendKey);

            const auto spendKeys = subWallets->m_publicSpendKeys;

            /* See if the derived spend key is one of ours */
            const auto it = std::find(spendKeys.begin(), spendKeys.end(), spendKey);

            if (it != spendKeys.end())
            {
                Crypto::PublicKey ourSpendKey = *it;

                WalletTypes::UnconfirmedInput input;

                input.amount = keyOutputs[outputIndex].amount;
                input.key = keyOutputs[outputIndex].key;
                input.parentTransactionHash = txHash;

                subWallets->storeUnconfirmedIncomingInput(input, ourSpendKey);
            }

            outputIndex++;
        }
    }

    void storeSentTransaction(
        const Crypto::Hash hash,
        const uint64_t fee,
        const std::string paymentID,
        const std::vector<WalletTypes::TxInputAndOwner> ourInputs,
        const std::string changeAddress,
        const uint64_t changeRequired,
        const std::shared_ptr<SubWallets> subWallets)
    {
        std::unordered_map<Crypto::PublicKey, int64_t> transfers;

        /* Loop through each input, and minus that from the transfers array */
        for (const auto &input : ourInputs)
        {
            transfers[input.publicSpendKey] -= input.input.amount;
        }

        /* Grab the subwallet the change address points to */
        const auto [spendKey, viewKey] = Utilities::addressToKeys(changeAddress);

        /* Increment the change address with the amount we returned to ourselves */
        if (changeRequired != 0)
        {
            transfers[spendKey] += changeRequired;
        }

        /* Not initialized till it's in a block */
        const uint64_t timestamp(0), blockHeight(0), unlockTime(0);

        const bool isCoinbaseTransaction = false;

        /* Create the unconfirmed transaction (Will be overwritten by the
           confirmed transaction later) */
        WalletTypes::Transaction tx(
            transfers, hash, fee, timestamp, blockHeight, paymentID, unlockTime, isCoinbaseTransaction);

        subWallets->addUnconfirmedTransaction(tx);
    }

    std::tuple<Error, Crypto::Hash>
        relayTransaction(const CryptoNote::Transaction tx, const std::shared_ptr<Nigel> daemon)
    {
        const auto [success, connectionError, error] = daemon->sendTransaction(tx);

        if (connectionError)
        {
            return {DAEMON_OFFLINE, Crypto::Hash()};
        }

        if (!success)
        {
            return {Error(DAEMON_ERROR, error), Crypto::Hash()};
        }

        return {SUCCESS, getTransactionHash(tx)};
    }

    std::vector<WalletTypes::TransactionDestination> setupDestinations(
        std::vector<std::pair<std::string, uint64_t>> addressesAndAmounts,
        const uint64_t changeRequired,
        const std::string changeAddress)
    {
        /* Need to send change back to our own address */
        if (changeRequired != 0)
        {
            addressesAndAmounts.push_back({changeAddress, changeRequired});
        }

        std::vector<WalletTypes::TransactionDestination> destinations;

        for (const auto &[address, amount] : addressesAndAmounts)
        {
            /* Grab the public keys from the receiver address */
            const auto [publicSpendKey, publicViewKey] = Utilities::addressToKeys(address);

            /* Split transfer into denominations and create an output for each */
            for (const auto denomination : splitAmountIntoDenominations(amount))
            {
                WalletTypes::TransactionDestination destination;

                destination.amount = denomination;
                destination.receiverPublicSpendKey = publicSpendKey;
                destination.receiverPublicViewKey = publicViewKey;

                destinations.push_back(destination);
            }
        }

        return destinations;
    }

    std::tuple<Error, std::vector<WalletTypes::RandomOuts>> getRingParticipants(
        const uint64_t mixin,
        const std::shared_ptr<Nigel> daemon,
        const std::vector<WalletTypes::TxInputAndOwner> sources)
    {
        /* Request one more than our mixin, then if we get our output as one of
           the mixin outs, we can skip it and still form the transaction */
        uint64_t requestedOuts = mixin + 1;

        if (mixin == 0)
        {
            return {SUCCESS, {}};
        }

        std::vector<uint64_t> amounts;

        /* Add each amount to the request vector */
        std::transform(sources.begin(), sources.end(), std::back_inserter(amounts), [](const auto destination) {
            return destination.input.amount;
        });

        const auto [success, fakeOuts] = daemon->getRandomOutsByAmounts(amounts, requestedOuts);

        if (!success)
        {
            return {DAEMON_OFFLINE, fakeOuts};
        }

        /* Verify outputs are sufficient */
        for (const uint64_t amount : amounts)
        {
            const auto it = std::find_if(
                fakeOuts.begin(), fakeOuts.end(), [amount](const auto output) { return output.amount == amount; });

            /* Item is not present at all */
            if (it == fakeOuts.end())
            {
                std::stringstream error;

                error << "Failed to get any matching outputs for amount " << amount << " ("
                      << Utilities::formatAmount(amount) << "). Further explanation here: "
                      << "https://gist.github.com/zpalmtree/80b3e80463225bcfb8f8432043cb594c";

                return {Error(NOT_ENOUGH_FAKE_OUTPUTS, error.str()), fakeOuts};
            }

            /* Check we have at least mixin outputs for each fake out. We *may* need
               mixin+1 outs for some, in case our real output gets included. This is
               unlikely though, and so we will error out down the line instead of here. */
            if (it->outputs.size() < mixin)
            {
                std::stringstream error;

                error << "Failed to get enough matching outputs for amount " << amount << " ("
                      << Utilities::formatAmount(amount) << "). Requested outputs: " << requestedOuts
                      << ", found outputs: " << it->outputs.size() << ". Further explanation here: "
                      << "https://gist.github.com/zpalmtree/80b3e80463225bcfb8f8432043cb594c";

                return {Error(NOT_ENOUGH_FAKE_OUTPUTS, error.str()), fakeOuts};
            }
        }

        if (fakeOuts.size() != amounts.size())
        {
            return {NOT_ENOUGH_FAKE_OUTPUTS, fakeOuts};
        }

        for (auto fakeOut : fakeOuts)
        {
            /* Do the same check as above here, again. The reason being that
               we just find the first set of outputs matching the amount above,
               and if we requests, say, outputs for the amount 100 twice, the
               first set might be sufficient, but the second are not.

               We could just check here instead of checking above, but then we
               might hit the length message first. Checking this way gives more
               informative errors. */
            if (fakeOut.outputs.size() < mixin)
            {
                std::stringstream error;

                error << "Failed to get enough matching outputs for amount " << fakeOut.amount << " ("
                      << Utilities::formatAmount(fakeOut.amount) << "). Requested outputs: " << requestedOuts
                      << ", found outputs: " << fakeOut.outputs.size() << ". Further explanation here: "
                      << "https://gist.github.com/zpalmtree/80b3e80463225bcfb8f8432043cb594c";

                return {Error(NOT_ENOUGH_FAKE_OUTPUTS, error.str()), fakeOuts};
            }

            /* Sort the fake outputs by their indexes (don't want there to be an
               easy way to determine which output is the real one) */
            std::sort(fakeOut.outputs.begin(), fakeOut.outputs.end(), [](const auto &lhs, const auto &rhs) {
                return lhs.index < rhs.index;
            });
        }

        return {SUCCESS, fakeOuts};
    }

    /* Take our inputs and pad them with fake inputs, based on our mixin value */
    std::tuple<Error, std::vector<WalletTypes::ObscuredInput>> prepareRingParticipants(
        std::vector<WalletTypes::TxInputAndOwner> sources,
        const uint64_t mixin,
        const std::shared_ptr<Nigel> daemon)
    {
        /* Sort our inputs by amount so they match up with the values we get
           back from the daemon */
        std::sort(sources.begin(), sources.end(), [](const auto &lhs, const auto &rhs) {
            return lhs.input.amount < rhs.input.amount;
        });

        std::vector<WalletTypes::ObscuredInput> result;

        const auto [error, fakeOuts] = getRingParticipants(mixin, daemon, sources);

        if (error)
        {
            return {error, result};
        }

        size_t i = 0;

        for (const auto &walletAmount : sources)
        {
            WalletTypes::GlobalIndexKey realOutput {*walletAmount.input.globalOutputIndex, walletAmount.input.key};

            WalletTypes::ObscuredInput obscuredInput;

            /* The real public key of the transaction */
            obscuredInput.realTransactionPublicKey = walletAmount.input.transactionPublicKey;

            /* The real index of the transaction output index */
            obscuredInput.realOutputTransactionIndex = walletAmount.input.transactionIndex;

            /* The amount of the transaction */
            obscuredInput.amount = walletAmount.input.amount;

            obscuredInput.ownerPublicSpendKey = walletAmount.publicSpendKey;

            obscuredInput.ownerPrivateSpendKey = walletAmount.privateSpendKey;

            obscuredInput.keyImage = walletAmount.input.keyImage;

            obscuredInput.privateEphemeral = walletAmount.input.privateEphemeral;

            if (mixin != 0)
            {
                /* Add the fake outputs to the transaction */
                for (const auto fakeOut : fakeOuts[i].outputs)
                {
                    /* This fake output is our output! Skip. */
                    if (walletAmount.input.globalOutputIndex == fakeOut.index)
                    {
                        continue;
                    }

                    /* Add the fake output */
                    obscuredInput.outputs.push_back({fakeOut.index, fakeOut.key});

                    /* Found enough fake outputs, we're done */
                    if (obscuredInput.outputs.size() >= mixin)
                    {
                        break;
                    }
                }
            }

            /* Didn't get enough fake outs to meet the mixin criteria */
            if (obscuredInput.outputs.size() < mixin)
            {
                std::stringstream error;

                error << "Failed to get enough matching outputs for amount " << walletAmount.input.amount << " ("
                      << Utilities::formatAmount(walletAmount.input.amount) << "). Requested outputs: " << mixin
                      << ", found outputs: " << obscuredInput.outputs.size() << ". Further explanation here: "
                      << "https://gist.github.com/zpalmtree/80b3e80463225bcfb8f8432043cb594c";

                return {Error(NOT_ENOUGH_FAKE_OUTPUTS, error.str()), result};
            }

            /* Find the position where our real input belongs, e.g. if the fake
               outputs have globalOutputIndexes of [1, 3, 5, 7], and our input
               has a globalOutputIndex of 6, we would place it like so:
               [1, 3, 5, 6, 7]. */
            const auto insertPosition = std::find_if(
                obscuredInput.outputs.begin(), obscuredInput.outputs.end(), [&walletAmount](const auto output) {
                    return output.index >= walletAmount.input.globalOutputIndex;
                });

            /* Insert our real output among the fakes */
            auto newPosition = obscuredInput.outputs.insert(insertPosition, realOutput);

            /* Indicate which of the outputs is the real one, e.g. number 4 */
            obscuredInput.realOutput = newPosition - obscuredInput.outputs.begin();

            result.push_back(obscuredInput);

            i++;
        }

        return {SUCCESS, result};
    }

    std::tuple<Error, std::vector<CryptoNote::KeyInput>, std::vector<Crypto::SecretKey>> setupInputs(
        const std::vector<WalletTypes::ObscuredInput> inputsAndFakes,
        const Crypto::SecretKey privateViewKey)
    {
        std::vector<CryptoNote::KeyInput> inputs;

        std::vector<Crypto::SecretKey> tmpSecretKeys;

        int numPregenerated = 0;
        int numGeneratedOnDemand = 0;

        for (auto input : inputsAndFakes)
        {
            CryptoNote::KeyInput keyInput;

            keyInput.amount = input.amount;
            keyInput.keyImage = input.keyImage;

            if (!input.privateEphemeral)
            {
                Crypto::KeyDerivation derivation;

                /* Derive the key from the transaction public key, and our private
                   view key */
                Crypto::generate_key_derivation(input.realTransactionPublicKey, privateViewKey, derivation);

                Crypto::SecretKey privateEphemeral;

                /* Derive the privateEphemeral */
                Crypto::derive_secret_key(
                    derivation, input.realOutputTransactionIndex, input.ownerPrivateSpendKey, privateEphemeral);

                input.privateEphemeral = privateEphemeral;

                numGeneratedOnDemand++;
            }
            else
            {
                numPregenerated++;
            }

            tmpSecretKeys.push_back(*input.privateEphemeral);

            /* Add each output index from the fake outs */
            std::transform(
                input.outputs.begin(),
                input.outputs.end(),
                std::back_inserter(keyInput.outputIndexes),
                [](const auto output) { return static_cast<uint32_t>(output.index); });

            /* Make a copy */
            auto copy = keyInput.outputIndexes;

            if (!keyInput.outputIndexes.empty())
            {
                /* Convert our indexes to relative indexes - for example, if we
                   originally had [5, 10, 20, 21, 22], this would become
                   [5, 5, 10, 1, 1]. Due to this, the indexes MUST be sorted - they
                   are serialized as a uint32_t, so negative values will overflow! */
                for (size_t i = 1; i < copy.size(); i++)
                {
                    copy[i] = keyInput.outputIndexes[i] - keyInput.outputIndexes[i - 1];
                }

                keyInput.outputIndexes = copy;
            }

            /* Store the key input */
            inputs.push_back(keyInput);
        }

        Logger::logger.log(
            "Generated private ephemerals for " + std::to_string(numGeneratedOnDemand) + " inputs, "
            "used pre-generated ephemerals for " + std::to_string(numPregenerated) + " inputs.",
            Logger::DEBUG,
            { Logger::TRANSACTIONS }
        );

        return {SUCCESS, inputs, tmpSecretKeys};
    }

    std::tuple<std::vector<WalletTypes::KeyOutput>, CryptoNote::KeyPair>
        setupOutputs(std::vector<WalletTypes::TransactionDestination> destinations)
    {
        /* Sort the destinations by amount. Helps obscure which output belongs to
           which transaction */
        std::sort(destinations.begin(), destinations.end(), [](const auto &lhs, const auto &rhs) {
            return lhs.amount < rhs.amount;
        });

        /* Generate a random key pair for the transaction - public key gets added
           to tx extra */
        CryptoNote::KeyPair randomTxKey;
        Crypto::generate_keys(randomTxKey.publicKey, randomTxKey.secretKey);

        /* Index of the output */
        uint32_t outputIndex = 0;

        std::vector<WalletTypes::KeyOutput> outputs;

        for (const auto &destination : destinations)
        {
            Crypto::KeyDerivation derivation;

            /* Generate derivation from receiver public view key and random tx key */
            Crypto::generate_key_derivation(destination.receiverPublicViewKey, randomTxKey.secretKey, derivation);

            Crypto::PublicKey tmpPubKey;

            /* Derive the temporary public key */
            Crypto::derive_public_key(derivation, outputIndex, destination.receiverPublicSpendKey, tmpPubKey);

            WalletTypes::KeyOutput keyOutput;

            keyOutput.key = tmpPubKey;
            keyOutput.amount = destination.amount;

            outputs.push_back(keyOutput);

            outputIndex++;
        }

        return {outputs, randomTxKey};
    }

    std::tuple<Error, CryptoNote::Transaction> generateRingSignatures(
        CryptoNote::Transaction tx,
        const std::vector<WalletTypes::ObscuredInput> inputsAndFakes,
        const std::vector<Crypto::SecretKey> tmpSecretKeys)
    {
        /* Hash the transaction prefix (Prefix is just a subset of transaction, so
           we can just do a cast here) */
        Crypto::Hash txPrefixHash = getTransactionHash(static_cast<CryptoNote::TransactionPrefix>(tx));

        size_t i = 0;

        /* Add the transaction signatures */
        for (const auto &input : inputsAndFakes)
        {
            std::vector<Crypto::PublicKey> publicKeys;

            /* Add all the fake outs public keys to a vector */
            for (const auto output : input.outputs)
            {
                publicKeys.push_back(output.key);
            }

            /* Generate the ring signatures - note - modifying the transaction
               post signature generation will invalidate the signatures. */
            const auto [success, signatures] = Crypto::crypto_ops::generateRingSignatures(
                txPrefixHash,
                boost::get<CryptoNote::KeyInput>(tx.inputs[i]).keyImage,
                publicKeys,
                tmpSecretKeys[i],
                input.realOutput);

            if (!success)
            {
                return {FAILED_TO_CREATE_RING_SIGNATURE, tx};
            }

            /* Add the signatures to the transaction */
            tx.signatures.push_back(signatures);

            i++;
        }

        i = 0;

        for (const auto &input : inputsAndFakes)
        {
            std::vector<Crypto::PublicKey> publicKeys;

            for (const auto &output : input.outputs)
            {
                publicKeys.push_back(output.key);
            }

            if (!Crypto::crypto_ops::checkRingSignature(
                    txPrefixHash,
                    boost::get<CryptoNote::KeyInput>(tx.inputs[i]).keyImage,
                    publicKeys,
                    tx.signatures[i]))
            {
                return {FAILED_TO_CREATE_RING_SIGNATURE, tx};
            }

            i++;
        }

        return {SUCCESS, tx};
    }

    /* Split each amount into uniform amounts, e.g.
       1234567 = 1000000 + 200000 + 30000 + 4000 + 500 + 60 + 7 */
    std::vector<uint64_t> splitAmountIntoDenominations(uint64_t amount, bool preventTooLargeOutputs)
    {
        std::vector<uint64_t> splitAmounts;

        uint64_t multiplier = 1;

        while (amount > 0)
        {
            uint64_t denomination = multiplier * (amount % 10);

            /* Denomination will make a too large output */
            if (denomination > CryptoNote::parameters::MAX_OUTPUT_SIZE_CLIENT && preventTooLargeOutputs)
            {
                /* Split amount into ten chunks */
                uint64_t numSplitAmounts = 10;

                uint64_t splitAmount = denomination / 10;

                /* If still too large, split each chunk into another 10 */
                while (splitAmount > CryptoNote::parameters::MAX_OUTPUT_SIZE_CLIENT)
                {
                    splitAmount /= 10;
                    numSplitAmounts *= 10;
                }

                /* Add the split amounts */
                std::fill_n(std::back_inserter(splitAmounts), numSplitAmounts, splitAmount);
            }
            /* If we have for example, 1010 - we want 1000 + 10,
               not 1000 + 0 + 10 + 0 */
            else if (denomination != 0)
            {
                splitAmounts.push_back(denomination);
            }

            amount /= 10;

            multiplier *= 10;
        }

        return splitAmounts;
    }

    std::vector<CryptoNote::TransactionInput>
        keyInputToTransactionInput(const std::vector<CryptoNote::KeyInput> keyInputs)
    {
        std::vector<CryptoNote::TransactionInput> result;

        for (const auto &input : keyInputs)
        {
            result.push_back(input);
        }

        return result;
    }

    std::vector<CryptoNote::TransactionOutput>
        keyOutputToTransactionOutput(const std::vector<WalletTypes::KeyOutput> keyOutputs)
    {
        std::vector<CryptoNote::TransactionOutput> result;

        for (const auto output : keyOutputs)
        {
            CryptoNote::TransactionOutput tmpOutput;

            tmpOutput.amount = output.amount;

            CryptoNote::KeyOutput tmpKey;

            tmpKey.key = output.key;

            tmpOutput.target = tmpKey;

            result.push_back(tmpOutput);
        }

        return result;
    }

    WalletTypes::TransactionResult makeTransaction(
        const uint64_t mixin,
        const std::shared_ptr<Nigel> daemon,
        const std::vector<WalletTypes::TxInputAndOwner> ourInputs,
        const std::string paymentID,
        const std::vector<WalletTypes::TransactionDestination> destinations,
        const std::shared_ptr<SubWallets> subWallets,
        const uint64_t unlockTime,
        const std::vector<uint8_t> extraData)
    {
        /* Mix our inputs with fake ones from the network to hide who we are */
        const auto [mixinError, inputsAndFakes] = prepareRingParticipants(ourInputs, mixin, daemon);

        WalletTypes::TransactionResult result;

        if (mixinError)
        {
            result.error = mixinError;
            return result;
        }

        /* Setup the transaction inputs */
        const auto [inputError, transactionInputs, tmpSecretKeys] =
            setupInputs(inputsAndFakes, subWallets->getPrivateViewKey());

        if (inputError)
        {
            result.error = inputError;
            return result;
        }

        /* Setup the transaction outputs */
        std::tie(result.outputs, result.txKeyPair) = setupOutputs(destinations);

        std::vector<uint8_t> extraNonce;

        if (paymentID != "")
        {
            Crypto::Hash paymentIDBin;

            Common::podFromHex(paymentID, paymentIDBin);

            /* Indicate this is the payment ID */
            extraNonce.push_back(Constants::TX_EXTRA_PAYMENT_ID_IDENTIFIER);

            /* Write the data to the extra nonce */
            std::copy(std::begin(paymentIDBin.data), std::end(paymentIDBin.data), std::back_inserter(extraNonce));
        }

        if (!extraData.empty())
        {
            /* Indicate this is arbitrary data */
            extraNonce.push_back(Constants::TX_EXTRA_ARBITRARY_DATA_IDENTIFIER);

            /* Determine the length of the data and varint encode it */
            std::vector<uint8_t> extraDataSize = Tools::uintToVarintVector(extraData.size());

            /* Write the length of the data out to extra */
            std::copy(extraDataSize.begin(), extraDataSize.end(), std::back_inserter(extraNonce));

            /* Write the data to the extra nonce */
            std::copy(extraData.begin(), extraData.end(), std::back_inserter(extraNonce));
        }

        std::vector<uint8_t> extra;

        if (!extraNonce.empty())
        {
            /* Indicate this is the extra nonce */
            extra.push_back(Constants::TX_EXTRA_NONCE_IDENTIFIER);

            /* Determine the length of the nonce data and varint encode it */
            std::vector<uint8_t> extraNonceSize = Tools::uintToVarintVector(extraNonce.size());

            /* Write the extra nonce length to extra */
            std::copy(extraNonceSize.begin(), extraNonceSize.end(), std::back_inserter(extra));

            /* Write the data to extra */
            std::copy(extraNonce.begin(), extraNonce.end(), std::back_inserter(extra));
        }

        /* Add the pub key identifier to extra */
        extra.push_back(Constants::TX_EXTRA_PUBKEY_IDENTIFIER);

        const auto pubKey = result.txKeyPair.publicKey;

        /* Append the pub key to extra */
        std::copy(std::begin(pubKey.data), std::end(pubKey.data), std::back_inserter(extra));

        CryptoNote::Transaction setupTX;

        setupTX.version = CryptoNote::CURRENT_TRANSACTION_VERSION;

        setupTX.unlockTime = unlockTime;

        /* Convert from key inputs to the boost uglyness */
        setupTX.inputs = keyInputToTransactionInput(transactionInputs);

        /* We can't really remove boost from here yet and simplify our data types
           since we take a hash of the transaction prefix. Once we've got this
           working, maybe we can work some magic. TODO */
        setupTX.outputs = keyOutputToTransactionOutput(result.outputs);

        if (setupTX.outputs.size() > CryptoNote::parameters::NORMAL_TX_MAX_OUTPUT_COUNT_V1)
        {
            result.error = OUTPUT_DECOMPOSITION;
            return result;
        }

        /* Pubkey, payment ID */
        setupTX.extra = extra;

        /* Fill in the transaction signatures */
        /* NOTE: Do not modify the transaction after this, or the ring signatures
           will be invalidated */
        std::tie(result.error, result.transaction) = generateRingSignatures(setupTX, inputsAndFakes, tmpSecretKeys);

        return result;
    }

    bool verifyAmounts(const CryptoNote::Transaction tx)
    {
        std::vector<uint64_t> amounts;

        /* Note - not verifying inputs as it's possible to have received inputs
           from another wallet which don't enforce this rule */
        for (const auto &output : tx.outputs)
        {
            amounts.push_back(output.amount);
        }

        return verifyAmounts(amounts);
    }

    /* Verify all amounts are valid amounts to send - that they are in PRETTY_AMOUNTS */
    bool verifyAmounts(const std::vector<uint64_t> amounts)
    {
        for (const auto amount : amounts)
        {
            if (std::find(Constants::PRETTY_AMOUNTS.begin(), Constants::PRETTY_AMOUNTS.end(), amount)
                == Constants::PRETTY_AMOUNTS.end())
            {
                return false;
            }
        }

        return true;
    }

    uint64_t sumTransactionFee(const CryptoNote::Transaction tx)
    {
        uint64_t inputTotal = 0;
        uint64_t outputTotal = 0;

        for (const auto &input : tx.inputs)
        {
            inputTotal += boost::get<CryptoNote::KeyInput>(input).amount;
        }

        for (const auto &output : tx.outputs)
        {
            outputTotal += output.amount;
        }

        return inputTotal - outputTotal;
    }

    bool verifyTransactionFee(
        const WalletTypes::FeeType expectedFee,
        const uint64_t actualFee,
        const CryptoNote::Transaction tx)
    {
        if (expectedFee.isFixedFee)
        {
            return expectedFee.fixedFee == actualFee;
        }
        else
        {
            const double feePerByte = expectedFee.isFeePerByte
                ? expectedFee.feePerByte
                : CryptoNote::parameters::MINIMUM_FEE_PER_BYTE_V1;

            const size_t txSize = toBinaryArray(tx).size();

            const size_t calculatedFee = static_cast<uint64_t>(feePerByte * txSize);

            /* Ensure fee is greater or equal to the fee per byte specified,
             * and no more than two times the fee per byte specified. */
            return actualFee >= calculatedFee && actualFee <= calculatedFee * 2;
        }
    }

} // namespace SendTransaction
