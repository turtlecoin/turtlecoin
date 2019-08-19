// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "AddBlockErrors.h"
#include "BlockValidationErrors.h"
#include "TransactionValidationErrors.h"

#include <string>
#include <system_error>

namespace CryptoNote
{
    namespace error
    {
        enum class AddBlockErrorCondition
        {
            BLOCK_ADDED = 0,
            BLOCK_REJECTED,
            BLOCK_VALIDATION_FAILED,
            TRANSACTION_VALIDATION_FAILED,
            DESERIALIZATION_FAILED
        };

        class AddBlockErrorConditionCategory : public std::error_category
        {
          public:
            static AddBlockErrorConditionCategory INSTANCE;

            virtual const char *name() const throw() override
            {
                return "AddBlockErrorCondition";
            }

            virtual std::error_condition default_error_condition(int ev) const throw() override
            {
                return std::error_condition(ev, *this);
            }

            virtual std::string message(int ev) const override
            {
                AddBlockErrorCondition code = static_cast<AddBlockErrorCondition>(ev);

                switch (code)
                {
                    case AddBlockErrorCondition::BLOCK_ADDED:
                        return "Block successfully added";
                    case AddBlockErrorCondition::BLOCK_REJECTED:
                        return "Block rejected";
                    case AddBlockErrorCondition::BLOCK_VALIDATION_FAILED:
                        return "Block validation failed";
                    case AddBlockErrorCondition::TRANSACTION_VALIDATION_FAILED:
                        return "Transaction validation failed";
                    default:
                        return "Unknown error condition";
                }
            }

            virtual bool equivalent(const std::error_code &errorCode, int condition) const throw() override
            {
                AddBlockErrorCondition code = static_cast<AddBlockErrorCondition>(condition);

                switch (code)
                {
                    case AddBlockErrorCondition::BLOCK_ADDED:
                        return errorCode == AddBlockErrorCode::ADDED_TO_MAIN
                               || errorCode == AddBlockErrorCode::ADDED_TO_ALTERNATIVE
                               || errorCode == AddBlockErrorCode::ADDED_TO_ALTERNATIVE_AND_SWITCHED
                               || errorCode == AddBlockErrorCode::ALREADY_EXISTS;

                    case AddBlockErrorCondition::DESERIALIZATION_FAILED:
                        return errorCode == AddBlockErrorCode::DESERIALIZATION_FAILED;

                    case AddBlockErrorCondition::BLOCK_REJECTED:
                        return errorCode == AddBlockErrorCode::REJECTED_AS_ORPHANED;

                    case AddBlockErrorCondition::BLOCK_VALIDATION_FAILED:
                        return &errorCode.category() == &BlockValidationErrorCategory::INSTANCE;

                    case AddBlockErrorCondition::TRANSACTION_VALIDATION_FAILED:
                        return &errorCode.category() == &TransactionValidationErrorCategory::INSTANCE;

                    default:
                        return false;
                }
            }
        };

        std::error_condition make_error_condition(AddBlockErrorCondition e);

    } // namespace error
} // namespace CryptoNote

namespace std
{
    template<> struct is_error_condition_enum<CryptoNote::error::AddBlockErrorCondition> : public true_type
    {
    };

} // namespace std
