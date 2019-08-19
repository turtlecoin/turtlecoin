// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "AddBlockErrorCondition.h"

namespace CryptoNote
{
    namespace error
    {
        AddBlockErrorConditionCategory AddBlockErrorConditionCategory::INSTANCE;

        std::error_condition make_error_condition(AddBlockErrorCondition e)
        {
            return std::error_condition(static_cast<int>(e), AddBlockErrorConditionCategory::INSTANCE);
        }

    } // namespace error
} // namespace CryptoNote
