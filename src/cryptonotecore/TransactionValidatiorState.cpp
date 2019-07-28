// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "TransactionValidatiorState.h"

namespace CryptoNote
{
    void mergeStates(TransactionValidatorState &destination, const TransactionValidatorState &source)
    {
        destination.spentKeyImages.insert(source.spentKeyImages.begin(), source.spentKeyImages.end());
    }

    bool hasIntersections(const TransactionValidatorState &destination, const TransactionValidatorState &source)
    {
        return std::any_of(source.spentKeyImages.begin(), source.spentKeyImages.end(), [&](const Crypto::KeyImage &ki) {
            return destination.spentKeyImages.count(ki) != 0;
        });
    }

    void excludeFromState(TransactionValidatorState &state, const CachedTransaction &cachedTransaction)
    {
        const auto &transaction = cachedTransaction.getTransaction();
        for (auto &input : transaction.inputs)
        {
            if (input.type() == typeid(KeyInput))
            {
                const auto &in = boost::get<KeyInput>(input);
                assert(state.spentKeyImages.count(in.keyImage) > 0);
                state.spentKeyImages.erase(in.keyImage);
            }
            else
            {
                assert(false);
            }
        }
    }

} // namespace CryptoNote
