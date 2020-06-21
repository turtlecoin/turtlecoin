// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
// Copyright (c) 2018, The Galaxia Project Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <string>
#include <system_error>

namespace CryptoNote
{
    namespace error
    {
        enum class TransactionValidationError
        {
            VALIDATION_SUCCESS = 0,
            EMPTY_INPUTS,
            INPUT_UNKNOWN_TYPE,
            INPUT_EMPTY_OUTPUT_USAGE,
            INPUT_INVALID_DOMAIN_KEYIMAGES,
            INPUT_IDENTICAL_KEYIMAGES,
            INPUT_IDENTICAL_OUTPUT_INDEXES,
            INPUT_KEYIMAGE_ALREADY_SPENT,
            INPUT_INVALID_GLOBAL_INDEX,
            INPUT_SPEND_LOCKED_OUT,
            INPUT_INVALID_SIGNATURES,
            INPUT_WRONG_SIGNATURES_COUNT,
            INPUTS_AMOUNT_OVERFLOW,
            INPUT_WRONG_COUNT,
            INPUT_UNEXPECTED_TYPE,
            BASE_INPUT_WRONG_BLOCK_INDEX,
            OUTPUT_ZERO_AMOUNT,
            OUTPUT_INVALID_KEY,
            OUTPUT_INVALID_REQUIRED_SIGNATURES_COUNT,
            OUTPUT_UNKNOWN_TYPE,
            OUTPUTS_AMOUNT_OVERFLOW,
            WRONG_AMOUNT,
            WRONG_TRANSACTION_UNLOCK_TIME,
            INVALID_MIXIN,
            EXTRA_TOO_LARGE,
            BASE_INVALID_SIGNATURES_COUNT,
            INPUT_INVALID_SIGNATURES_COUNT,
            OUTPUT_AMOUNT_TOO_LARGE,
            EXCESSIVE_OUTPUTS,
            WRONG_FEE,
            SIZE_TOO_LARGE,
            MINER_OUTPUT_NOT_CLAIMED
        };

        // custom category:
        class TransactionValidationErrorCategory : public std::error_category
        {
          public:
            static TransactionValidationErrorCategory INSTANCE;

            virtual const char *name() const throw()
            {
                return "TransactionValidationErrorCategory";
            }

            virtual std::error_condition default_error_condition(int ev) const throw()
            {
                return std::error_condition(ev, *this);
            }

            virtual std::string message(int ev) const
            {
                TransactionValidationError code = static_cast<TransactionValidationError>(ev);

                switch (code)
                {
                    case TransactionValidationError::VALIDATION_SUCCESS:
                        return "Transaction successfully validated";
                    case TransactionValidationError::EMPTY_INPUTS:
                        return "Transaction has no inputs";
                    case TransactionValidationError::INPUT_UNKNOWN_TYPE:
                        return "Transaction has input with unknown type";
                    case TransactionValidationError::INPUT_EMPTY_OUTPUT_USAGE:
                        return "Transaction's input uses empty output";
                    case TransactionValidationError::INPUT_INVALID_DOMAIN_KEYIMAGES:
                        return "Transaction uses key image not in the valid domain";
                    case TransactionValidationError::INPUT_IDENTICAL_KEYIMAGES:
                        return "Transaction has identical key images";
                    case TransactionValidationError::INPUT_IDENTICAL_OUTPUT_INDEXES:
                        return "Transaction has identical output indexes";
                    case TransactionValidationError::INPUT_KEYIMAGE_ALREADY_SPENT:
                        return "Transaction contains an input which has already been spent";
                    case TransactionValidationError::INPUT_INVALID_GLOBAL_INDEX:
                        return "Transaction has input with invalid global index";
                    case TransactionValidationError::INPUT_SPEND_LOCKED_OUT:
                        return "Transaction uses locked input";
                    case TransactionValidationError::INPUT_INVALID_SIGNATURES:
                        return "Transaction has input with invalid signature";
                    case TransactionValidationError::INPUT_WRONG_SIGNATURES_COUNT:
                        return "Transaction has input with wrong signatures count";
                    case TransactionValidationError::INPUTS_AMOUNT_OVERFLOW:
                        return "Transaction's inputs sum overflow";
                    case TransactionValidationError::INPUT_WRONG_COUNT:
                        return "Wrong input count";
                    case TransactionValidationError::INPUT_UNEXPECTED_TYPE:
                        return "Wrong input type";
                    case TransactionValidationError::BASE_INPUT_WRONG_BLOCK_INDEX:
                        return "Base input has wrong block index";
                    case TransactionValidationError::OUTPUT_ZERO_AMOUNT:
                        return "Transaction has zero output amount";
                    case TransactionValidationError::OUTPUT_INVALID_KEY:
                        return "Transaction has output with invalid key";
                    case TransactionValidationError::OUTPUT_INVALID_REQUIRED_SIGNATURES_COUNT:
                        return "Transaction has output with invalid signatures count";
                    case TransactionValidationError::OUTPUT_UNKNOWN_TYPE:
                        return "Transaction has unknown output type";
                    case TransactionValidationError::OUTPUTS_AMOUNT_OVERFLOW:
                        return "Transaction has outputs amount overflow";
                    case TransactionValidationError::WRONG_AMOUNT:
                        return "Transaction wrong amount";
                    case TransactionValidationError::WRONG_TRANSACTION_UNLOCK_TIME:
                        return "Transaction has wrong unlock time";
                    case TransactionValidationError::INVALID_MIXIN:
                        return "Mixin too large or too small";
                    case TransactionValidationError::EXTRA_TOO_LARGE:
                        return "Transaction extra too large";
                    case TransactionValidationError::BASE_INVALID_SIGNATURES_COUNT:
                        return "Coinbase transactions must not have input signatures";
                    case TransactionValidationError::INPUT_INVALID_SIGNATURES_COUNT:
                        return "The number of input signatures is not correct";
                    case TransactionValidationError::OUTPUT_AMOUNT_TOO_LARGE:
                        return "Transaction has output exceeding max output size";
                    case TransactionValidationError::EXCESSIVE_OUTPUTS:
                        return "Transaction has an excessive number of outputs. Reduce the number of payees.";
                    case TransactionValidationError::WRONG_FEE:
                        return "Transaction fee is below minimum fee and is not a fusion transaction";
                    case TransactionValidationError::SIZE_TOO_LARGE:
                        return "Transaction is too large (in bytes)";
                    case TransactionValidationError::MINER_OUTPUT_NOT_CLAIMED:
                        return "Coinbase transaction derived spend key does not match supplied public spend key in tx_extra.";
                    default:
                        return "Unknown error";
                }
            }

          private:
            TransactionValidationErrorCategory() {}
        };

        inline std::error_code make_error_code(CryptoNote::error::TransactionValidationError e)
        {
            return std::error_code(
                static_cast<int>(e), CryptoNote::error::TransactionValidationErrorCategory::INSTANCE);
        }

    } // namespace error
} // namespace CryptoNote

namespace std
{
    template<> struct is_error_code_enum<CryptoNote::error::TransactionValidationError> : public true_type
    {
    };

} // namespace std
