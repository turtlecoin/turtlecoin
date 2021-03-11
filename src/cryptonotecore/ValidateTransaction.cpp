// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The Galaxia Project Developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <config/CryptoNoteConfig.h>
#include <cryptonotecore/Mixins.h>
#include <cryptonotecore/TransactionValidationErrors.h>
#include <cryptonotecore/ValidateTransaction.h>
#include <utilities/Utilities.h>

ValidateTransaction::ValidateTransaction(
    const CryptoNote::CachedTransaction &cachedTransaction,
    CryptoNote::TransactionValidatorState &state,
    CryptoNote::IBlockchainCache *cache,
    const CryptoNote::Currency &currency,
    const CryptoNote::Checkpoints &checkpoints,
    Utilities::ThreadPool<bool> &threadPool,
    const uint64_t blockHeight,
    const uint64_t blockSizeMedian,
    const bool isPoolTransaction):
    m_cachedTransaction(cachedTransaction),
    m_transaction(cachedTransaction.getTransaction()),
    m_validatorState(state),
    m_currency(currency),
    m_checkpoints(checkpoints),
    m_threadPool(threadPool),
    m_blockchainCache(cache),
    m_blockHeight(blockHeight),
    m_blockSizeMedian(blockSizeMedian),
    m_isPoolTransaction(isPoolTransaction)
{
}

TransactionValidationResult ValidateTransaction::validate()
{
    /* Validate transaction isn't too big */
    if (!validateTransactionSize())
    {
        return m_validationResult;
    }

    /* Validate the transaction inputs are non empty, key images are valid, etc. */
    if (!validateTransactionInputs())
    {
        return m_validationResult;
    }

    /* Validate transaction outputs are non zero, don't overflow, etc */
    if (!validateTransactionOutputs())
    {
        return m_validationResult;
    }

    /* Verify inputs > outputs, fee is > min fee unless fusion, etc */
    if (!validateTransactionFee())
    {
        return m_validationResult;
    }

    /* Validate the transaction extra is a reasonable size. */
    if (!validateTransactionExtra())
    {
        return m_validationResult;
    }

    /* Validate transaction input / output ratio is not excessive */
    if (!validateInputOutputRatio())
    {
        return m_validationResult;
    }

    /* Validate transaction mixin is in the valid range */
    if (!validateTransactionMixin())
    {
        return m_validationResult;
    }

    /* Verify key images are not spent, ring signatures are valid, etc. We
     * do this separately from the transaction input verification, because
     * these checks are much slower to perform, so we want to fail fast on the
     * cheaper checks first. */
    if (!validateTransactionInputsExpensive())
    {
        return m_validationResult;
    }

    m_validationResult.valid = true;
    setTransactionValidationResult(CryptoNote::error::TransactionValidationError::VALIDATION_SUCCESS);

    return m_validationResult;
}

/* Note: Does not set the .fee property */
TransactionValidationResult ValidateTransaction::revalidateAfterHeightChange()
{
    /* Validate transaction isn't too big now that the median size has changed */
    if (!validateTransactionSize())
    {
        return m_validationResult;
    }

    /* Validate the transaction extra is still a reasonable size. */
    if (!validateTransactionExtra())
    {
        return m_validationResult;
    }

    /* Validate transaction mixin is still in the valid range */
    if (!validateTransactionMixin())
    {
        return m_validationResult;
    }

    m_validationResult.valid = true;
    setTransactionValidationResult(CryptoNote::error::TransactionValidationError::VALIDATION_SUCCESS);

    return m_validationResult;
}


bool ValidateTransaction::validateTransactionSize()
{
    const auto maxTransactionSize = m_blockSizeMedian * 2 - m_currency.minerTxBlobReservedSize();

    if (m_cachedTransaction.getTransactionBinaryArray().size() > maxTransactionSize)
    {
        setTransactionValidationResult(
            CryptoNote::error::TransactionValidationError::SIZE_TOO_LARGE, "Transaction is too large (in bytes)");

        return false;
    }

    return true;
}

bool ValidateTransaction::validateTransactionInputs()
{
    if (m_transaction.inputs.empty())
    {
        setTransactionValidationResult(
            CryptoNote::error::TransactionValidationError::EMPTY_INPUTS, "Transaction has no inputs");

        return false;
    }

    static const Crypto::KeyImage Z = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    static const Crypto::KeyImage I = {{0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

    static const Crypto::KeyImage L = {{0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7,
                                        0xa2, 0xde, 0xf9, 0xde, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10}};

    uint64_t sumOfInputs = 0;

    std::unordered_set<Crypto::KeyImage> ki;

    for (const auto &input : m_transaction.inputs)
    {
        uint64_t amount = 0;

        if (input.type() == typeid(CryptoNote::KeyInput))
        {
            const CryptoNote::KeyInput &in = boost::get<CryptoNote::KeyInput>(input);
            amount = in.amount;

            if (!ki.insert(in.keyImage).second)
            {
                setTransactionValidationResult(
                    CryptoNote::error::TransactionValidationError::INPUT_IDENTICAL_KEYIMAGES,
                    "Transaction contains identical key images: " + Common::podToHex(in.keyImage));

                return false;
            }

            if (in.outputIndexes.empty())
            {
                setTransactionValidationResult(
                    CryptoNote::error::TransactionValidationError::INPUT_EMPTY_OUTPUT_USAGE,
                    "Transaction contains no output indexes");

                return false;
            }

            /* outputIndexes are packed here, first is absolute, others are offsets to previous,
             * so first can be zero, others can't
             * Fix discovered by Monero Lab and suggested by "fluffypony" (bitcointalk.org) */
            if (!(scalarmultKey(in.keyImage, L) == I))
            {
                setTransactionValidationResult(
                    CryptoNote::error::TransactionValidationError::INPUT_INVALID_DOMAIN_KEYIMAGES,
                    "Transaction contains key images in an invalid domain: " + Common::podToHex(in.keyImage));

                return false;
            }

            if (std::find(++std::begin(in.outputIndexes), std::end(in.outputIndexes), 0) != std::end(in.outputIndexes))
            {
                setTransactionValidationResult(
                    CryptoNote::error::TransactionValidationError::INPUT_IDENTICAL_OUTPUT_INDEXES,
                    "Transaction contains identical output indexes");

                return false;
            }

            if (!m_validatorState.spentKeyImages.insert(in.keyImage).second)
            {
                setTransactionValidationResult(
                    CryptoNote::error::TransactionValidationError::INPUT_KEYIMAGE_ALREADY_SPENT,
                    "Transaction contains key image that has already been spent: " + Common::podToHex(in.keyImage));

                return false;
            }
        }
        else
        {
            setTransactionValidationResult(
                CryptoNote::error::TransactionValidationError::INPUT_UNKNOWN_TYPE,
                "Transaction input has an unknown input type");

            return false;
        }

        if (std::numeric_limits<uint64_t>::max() - amount < sumOfInputs)
        {
            setTransactionValidationResult(
                CryptoNote::error::TransactionValidationError::INPUTS_AMOUNT_OVERFLOW,
                "Transaction inputs will overflow");

            return false;
        }

        sumOfInputs += amount;
    }

    m_sumOfInputs = sumOfInputs;

    return true;
}

bool ValidateTransaction::validateTransactionOutputs()
{
    uint64_t sumOfOutputs = 0;

    for (const auto &output : m_transaction.outputs)
    {
        if (output.amount == 0)
        {
            setTransactionValidationResult(
                CryptoNote::error::TransactionValidationError::OUTPUT_ZERO_AMOUNT,
                "Transaction has an output amount of zero");

            return false;
        }

        if (m_blockHeight >= CryptoNote::parameters::MAX_OUTPUT_SIZE_HEIGHT)
        {
            if (output.amount > CryptoNote::parameters::MAX_OUTPUT_SIZE_NODE)
            {
                setTransactionValidationResult(
                    CryptoNote::error::TransactionValidationError::OUTPUT_AMOUNT_TOO_LARGE,
                    "Transaction has a too large output amount");

                return false;
            }
        }

        if (output.target.type() == typeid(CryptoNote::KeyOutput))
        {
            if (!check_key(boost::get<CryptoNote::KeyOutput>(output.target).key))
            {
                setTransactionValidationResult(
                    CryptoNote::error::TransactionValidationError::OUTPUT_INVALID_KEY,
                    "Transaction output has an invalid output key: " + Common::podToHex(boost::get<CryptoNote::KeyOutput>(output.target).key));

                return false;
            }
        }
        else
        {
            setTransactionValidationResult(
                CryptoNote::error::TransactionValidationError::OUTPUT_UNKNOWN_TYPE,
                "Transaction output has an unknown output type");

            return false;
        }

        if (std::numeric_limits<uint64_t>::max() - output.amount < sumOfOutputs)
        {
            setTransactionValidationResult(
                CryptoNote::error::TransactionValidationError::OUTPUTS_AMOUNT_OVERFLOW,
                "Transaction outputs will overflow");

            return false;
        }

        sumOfOutputs += output.amount;
    }

    m_sumOfOutputs = sumOfOutputs;

    return true;
}

/**
 * Pre-requisite - Call validateTransactionInputs() and validateTransactionOutputs()
 * to ensure m_sumOfInputs and m_sumOfOutputs is set
 */
bool ValidateTransaction::validateTransactionFee()
{
    if (m_sumOfInputs == 0)
    {
        throw std::runtime_error("Error! You must call validateTransactionInputs() and "
                                 "validateTransactionOutputs() before calling validateTransactionFee()!");
    }

    if (m_sumOfOutputs > m_sumOfInputs)
    {
        setTransactionValidationResult(
            CryptoNote::error::TransactionValidationError::WRONG_AMOUNT,
            "Sum of outputs is greater than sum of inputs");

        return false;
    }

    const uint64_t fee = m_sumOfInputs - m_sumOfOutputs;

    const bool isFusion = m_currency.isFusionTransaction(
        m_transaction, m_cachedTransaction.getTransactionBinaryArray().size(), m_blockHeight);

    if (!isFusion)
    {
        bool validFee = fee != 0;

        if (m_blockHeight >= CryptoNote::parameters::MINIMUM_FEE_PER_BYTE_V1_HEIGHT)
        {
            const auto minFee = Utilities::getMinimumTransactionFee(
                m_cachedTransaction.getTransactionBinaryArray().size(), m_blockHeight);

            validFee = fee >= minFee;
        }
        else if (m_isPoolTransaction)
        {
            validFee = fee >= CryptoNote::parameters::MINIMUM_FEE;
        }

        if (!validFee)
        {
            setTransactionValidationResult(
                CryptoNote::error::TransactionValidationError::WRONG_FEE,
                "Transaction fee is below minimum fee and is not a fusion transaction");

            return false;
        }
    }

    m_validationResult.fee = fee;

    return true;
}

bool ValidateTransaction::validateTransactionExtra()
{
    const uint64_t heightToEnforce =
        CryptoNote::parameters::MAX_EXTRA_SIZE_V2_HEIGHT + CryptoNote::parameters::CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW;

    /* If we're checking if it's valid for the pool, we don't wait for the height
     * to enforce. */
    if (m_isPoolTransaction || m_blockHeight >= heightToEnforce)
    {
        if (m_transaction.extra.size() >= CryptoNote::parameters::MAX_EXTRA_SIZE_V2)
        {
            setTransactionValidationResult(
                CryptoNote::error::TransactionValidationError::EXTRA_TOO_LARGE, "Transaction extra is too large");

            return false;
        }
    }

    return true;
}

bool ValidateTransaction::validateInputOutputRatio()
{
    if (m_isPoolTransaction || m_blockHeight >= CryptoNote::parameters::NORMAL_TX_MAX_OUTPUT_COUNT_V1_HEIGHT)
    {
        if (m_transaction.outputs.size() > CryptoNote::parameters::NORMAL_TX_MAX_OUTPUT_COUNT_V1)
        {
            setTransactionValidationResult(
                CryptoNote::error::TransactionValidationError::EXCESSIVE_OUTPUTS,
                "Transaction has excessive outputs. Reduce the number of payees.");

            return false;
        }
    }

    return true;
}

bool ValidateTransaction::validateTransactionMixin()
{
    /* This allows us to accept blocks with transaction mixins for the mined money unlock window
     * that may be using older mixin rules on the network. This helps to clear out the transaction
     * pool during a network soft fork that requires a mixin lower or upper bound change */
    uint32_t mixinChangeWindow = m_blockHeight;

    if (mixinChangeWindow > CryptoNote::parameters::CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW)
    {
        mixinChangeWindow = mixinChangeWindow - CryptoNote::parameters::CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW;
    }

    auto [success, err] = CryptoNote::Mixins::validate({m_cachedTransaction}, m_blockHeight);

    if (!success)
    {
        if (m_isPoolTransaction)
        {
            setTransactionValidationResult(CryptoNote::error::TransactionValidationError::INVALID_MIXIN, err);

            return false;
        }

        /* Warning, this shadows the above variables */
        auto [success, err] = CryptoNote::Mixins::validate({m_cachedTransaction}, m_blockHeight);

        if (!success)
        {
            setTransactionValidationResult(CryptoNote::error::TransactionValidationError::INVALID_MIXIN, err);

            return false;
        }
    }

    return true;
}

bool ValidateTransaction::validateTransactionInputsExpensive()
{
    /* Don't need to do expensive transaction validation for transactions
     * in a checkpoints range - they are assumed valid, and the transaction
     * hash would change thus invalidation the checkpoints if not. */
    if (m_checkpoints.isInCheckpointZone(m_blockHeight + 1))
    {
        return true;
    }

    uint64_t inputIndex = 0;

    std::vector<std::future<bool>> validationResult;
    std::atomic<bool> cancelValidation = false;
    const Crypto::Hash prefixHash = m_cachedTransaction.getTransactionPrefixHash();

    for (const auto &input : m_transaction.inputs)
    {
        /* Validate each input on a separate thread in our thread pool */
        validationResult.push_back(m_threadPool.addJob([inputIndex, &input, &prefixHash, &cancelValidation, this] {
            const CryptoNote::KeyInput &in = boost::get<CryptoNote::KeyInput>(input);
            if (cancelValidation)
            {
                return false; // fail the validation immediately if cancel requested
            }
            if (m_blockchainCache->checkIfSpent(in.keyImage, m_blockHeight))
            {
                setTransactionValidationResult(
                    CryptoNote::error::TransactionValidationError::INPUT_KEYIMAGE_ALREADY_SPENT,
                    "Transaction contains key image that has already been spent: " + Common::podToHex(in.keyImage));

                return false;
            }

            std::vector<Crypto::PublicKey> outputKeys;
            std::vector<uint32_t> globalIndexes(in.outputIndexes.size());

            globalIndexes[0] = in.outputIndexes[0];

            /* Convert output indexes from relative to absolute */
            for (size_t i = 1; i < in.outputIndexes.size(); ++i)
            {
                globalIndexes[i] = globalIndexes[i - 1] + in.outputIndexes[i];
            }

            const auto result = m_blockchainCache->extractKeyOutputKeys(
                in.amount, m_blockHeight, {globalIndexes.data(), globalIndexes.size()}, outputKeys);

            if (result == CryptoNote::ExtractOutputKeysResult::INVALID_GLOBAL_INDEX)
            {
                setTransactionValidationResult(
                    CryptoNote::error::TransactionValidationError::INPUT_INVALID_GLOBAL_INDEX,
                    "Transaction contains invalid global indexes");

                return false;
            }

            if (result == CryptoNote::ExtractOutputKeysResult::OUTPUT_LOCKED)
            {
                setTransactionValidationResult(
                    CryptoNote::error::TransactionValidationError::INPUT_SPEND_LOCKED_OUT,
                    "Transaction includes an input which is still locked");

                return false;
            }

            if (m_isPoolTransaction
                || m_blockHeight >= CryptoNote::parameters::TRANSACTION_SIGNATURE_COUNT_VALIDATION_HEIGHT)
            {
                if (outputKeys.size() != m_transaction.signatures[inputIndex].size())
                {
                    setTransactionValidationResult(
                        CryptoNote::error::TransactionValidationError::INPUT_INVALID_SIGNATURES_COUNT,
                        "Transaction has an invalid number of signatures");

                    return false;
                }
            }

            if (!Crypto::crypto_ops::checkRingSignature(
                    prefixHash, in.keyImage, outputKeys, m_transaction.signatures[inputIndex]))
            {
                setTransactionValidationResult(
                    CryptoNote::error::TransactionValidationError::INPUT_INVALID_SIGNATURES,
                    "Transaction contains invalid signatures: " + Common::podToHex(in.keyImage));

                return false;
            }

            return true;
        }));

        inputIndex++;
    }

    bool valid = true;

    for (auto &result : validationResult)
    {
        if (!result.get())
        {
            valid = false;
            cancelValidation = true;
        }
    }

    return valid;
}


void ValidateTransaction::setTransactionValidationResult(
    const std::error_code &error_code,
    const std::string &error_message)
{
    std::scoped_lock<std::mutex> lock(m_mutex);

    m_validationResult.errorCode = error_code;

    m_validationResult.errorMessage = error_message;
}
