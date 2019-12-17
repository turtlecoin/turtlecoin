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

ValidateTransaction::ValidateTransaction(
    const CryptoNote::CachedTransaction &cachedTransaction,
    CryptoNote::TransactionValidatorState &state,
    CryptoNote::IBlockchainCache *cache,
    const CryptoNote::Currency &currency,
    const CryptoNote::Checkpoints &checkpoints,
    Utilities::ThreadPool<bool> &threadPool,
    const uint64_t blockHeight,
    const uint64_t blockSizeMedian,
    const bool isPoolTransaction) :
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
    m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::VALIDATION_SUCCESS;

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
    m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::VALIDATION_SUCCESS;

    return m_validationResult;
}


bool ValidateTransaction::validateTransactionSize()
{
    const auto maxTransactionSize = m_blockSizeMedian * 2 - m_currency.minerTxBlobReservedSize();

    if (m_cachedTransaction.getTransactionBinaryArray().size() > maxTransactionSize)
    {
        m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::SIZE_TOO_LARGE;
        m_validationResult.errorMessage = "Transaction is too large (in bytes)";
        return false;
    }

    return true;
}

bool ValidateTransaction::validateTransactionInputs()
{
    if (m_transaction.inputs.empty())
    {
        m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::EMPTY_INPUTS;
        m_validationResult.errorMessage = "Transaction has no inputs";

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
                m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::INPUT_IDENTICAL_KEYIMAGES;
                m_validationResult.errorMessage = "Transaction contains identical key images";

                return false;
            }

            if (in.outputIndexes.empty())
            {
                m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::INPUT_EMPTY_OUTPUT_USAGE;
                m_validationResult.errorMessage = "Transaction contains no output indexes";

                return false;
            }

            /* outputIndexes are packed here, first is absolute, others are offsets to previous,
             * so first can be zero, others can't
             * Fix discovered by Monero Lab and suggested by "fluffypony" (bitcointalk.org) */
            if (!(scalarmultKey(in.keyImage, L) == I))
            {
                m_validationResult.errorCode =
                    CryptoNote::error::TransactionValidationError::INPUT_INVALID_DOMAIN_KEYIMAGES;
                m_validationResult.errorMessage = "Transaction contains key images in an invalid domain";

                return false;
            }

            if (std::find(++std::begin(in.outputIndexes), std::end(in.outputIndexes), 0) != std::end(in.outputIndexes))
            {
                m_validationResult.errorCode =
                    CryptoNote::error::TransactionValidationError::INPUT_IDENTICAL_OUTPUT_INDEXES;
                m_validationResult.errorMessage = "Transaction contains identical output indexes";

                return false;
            }

            if (!m_validatorState.spentKeyImages.insert(in.keyImage).second)
            {
                m_validationResult.errorCode =
                    CryptoNote::error::TransactionValidationError::INPUT_KEYIMAGE_ALREADY_SPENT;
                m_validationResult.errorMessage = "Transaction contains key image that has already been spent";

                return false;
            }
        }
        else
        {
            m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::INPUT_UNKNOWN_TYPE;
            m_validationResult.errorMessage = "Transaction input has an unknown input type";

            return false;
        }

        if (std::numeric_limits<uint64_t>::max() - amount < sumOfInputs)
        {
            m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::INPUTS_AMOUNT_OVERFLOW;
            m_validationResult.errorMessage = "Transaction inputs will overflow";

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
            m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::OUTPUT_ZERO_AMOUNT;
            m_validationResult.errorMessage = "Transaction has an output amount of zero";

            return false;
        }

        if (m_blockHeight >= CryptoNote::parameters::MAX_OUTPUT_SIZE_HEIGHT)
        {
            if (output.amount > CryptoNote::parameters::MAX_OUTPUT_SIZE_NODE)
            {
                m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::OUTPUT_AMOUNT_TOO_LARGE;
                m_validationResult.errorMessage = "Transaction has a too large output amount";

                return false;
            }
        }

        if (output.target.type() == typeid(CryptoNote::KeyOutput))
        {
            if (!check_key(boost::get<CryptoNote::KeyOutput>(output.target).key))
            {
                m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::OUTPUT_INVALID_KEY;
                m_validationResult.errorMessage = "Transaction output has an invalid output key";

                return false;
            }
        }
        else
        {
            m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::OUTPUT_UNKNOWN_TYPE;
            m_validationResult.errorMessage = "Transaction output has an unknown output type";

            return false;
        }

        if (std::numeric_limits<uint64_t>::max() - output.amount < sumOfOutputs)
        {
            m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::OUTPUTS_AMOUNT_OVERFLOW;
            m_validationResult.errorMessage = "Transaction outputs will overflow";

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
        m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::WRONG_AMOUNT;
        m_validationResult.errorMessage = "Sum of outputs is greater than sum of inputs";

        return false;
    }

    const uint64_t fee = m_sumOfInputs - m_sumOfOutputs;

    const bool isFusion = m_currency.isFusionTransaction(
        m_transaction, m_cachedTransaction.getTransactionBinaryArray().size(), m_blockHeight);

    if (!isFusion)
    {
        if (fee == 0 || (fee < CryptoNote::parameters::MINIMUM_FEE && m_isPoolTransaction))
        {
            m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::WRONG_FEE;
            m_validationResult.errorMessage = "Transaction fee is below minimum fee and is not a fusion transaction";

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
            m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::EXTRA_TOO_LARGE;
            m_validationResult.errorMessage = "Transaction extra is too large";

            return false;
        }
    }

    return true;
}

bool ValidateTransaction::validateInputOutputRatio()
{
    if (m_isPoolTransaction || m_blockHeight >= CryptoNote::parameters::NORMAL_TX_MAX_OUTPUT_RATIO_V1_HEIGHT)
    {
        if (m_transaction.outputs.size()
            > m_transaction.inputs.size() * CryptoNote::parameters::NORMAL_TX_MAX_OUTPUT_RATIO_V1)
        {
            m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::EXCESSIVE_OUTPUTS;
            m_validationResult.errorMessage = "Transaction has excessive output/input ratio";

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
            m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::INVALID_MIXIN;
            m_validationResult.errorMessage = err;

            return false;
        }

        /* Warning, this shadows the above variables */
        auto [success, err] = CryptoNote::Mixins::validate({m_cachedTransaction}, m_blockHeight);

        if (!success)
        {
            m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::INVALID_MIXIN;
            m_validationResult.errorMessage = err;

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
                m_validationResult.errorCode =
                    CryptoNote::error::TransactionValidationError::INPUT_KEYIMAGE_ALREADY_SPENT;
                m_validationResult.errorMessage = "Transaction contains key image that has already been spent";

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
                m_validationResult.errorCode =
                    CryptoNote::error::TransactionValidationError::INPUT_INVALID_GLOBAL_INDEX;
                m_validationResult.errorMessage = "Transaction contains invalid global indexes";

                return false;
            }

            if (result == CryptoNote::ExtractOutputKeysResult::OUTPUT_LOCKED)
            {
                m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::INPUT_SPEND_LOCKED_OUT;
                m_validationResult.errorMessage = "Transaction includes an input which is still locked";

                return false;
            }

            if (m_isPoolTransaction
                || m_blockHeight >= CryptoNote::parameters::TRANSACTION_SIGNATURE_COUNT_VALIDATION_HEIGHT)
            {
                if (outputKeys.size() != m_transaction.signatures[inputIndex].size())
                {
                    m_validationResult.errorCode =
                        CryptoNote::error::TransactionValidationError::INPUT_INVALID_SIGNATURES_COUNT;
                    m_validationResult.errorMessage = "Transaction has an invalid number of signatures";

                    return false;
                }
            }

            if (!Crypto::crypto_ops::checkRingSignature(
                    prefixHash, in.keyImage, outputKeys, m_transaction.signatures[inputIndex]))
            {
                m_validationResult.errorCode = CryptoNote::error::TransactionValidationError::INPUT_INVALID_SIGNATURES;
                m_validationResult.errorMessage = "Transaction contains invalid signatures";

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
