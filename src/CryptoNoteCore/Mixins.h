// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CachedTransaction.h"
#include "TransactionApi.h"
#include "Wallet/WalletErrors.h"
#include <config/CryptoNoteConfig.h>

namespace CryptoNote
{
  class Mixins
  {
    public:

      /* This method is used to get the minimum and maximum mixin permitted for the
         requested height */
      static std::tuple<uint64_t, uint64_t> getMixinAllowableRange(const uint32_t height)
      {
        uint64_t minMixin = 0;
        uint64_t maxMixin = std::numeric_limits<uint64_t>::max();

        /* We now limit the mixin allowed in a transaction. However, there have been
           some transactions outside these limits in the past, so we need to only
           enforce this on new blocks, otherwise wouldn't be able to sync the chain */

        /* We also need to ensure that the mixin enforced is for the limit that
           was correct when the block was formed - i.e. if 0 mixin was allowed at
           block 100, but is no longer allowed - we should still validate block 100 */

        if (height >= CryptoNote::parameters::MIXIN_LIMITS_V3_HEIGHT)
        {
          minMixin = CryptoNote::parameters::MINIMUM_MIXIN_V3;
          maxMixin = CryptoNote::parameters::MAXIMUM_MIXIN_V3;
        }
        else if (height >= CryptoNote::parameters::MIXIN_LIMITS_V2_HEIGHT)
        {
          minMixin = CryptoNote::parameters::MINIMUM_MIXIN_V2;
          maxMixin = CryptoNote::parameters::MAXIMUM_MIXIN_V2;
        }
        else if (height >= CryptoNote::parameters::MIXIN_LIMITS_V1_HEIGHT)
        {
          minMixin = CryptoNote::parameters::MINIMUM_MIXIN_V1;
          maxMixin = CryptoNote::parameters::MAXIMUM_MIXIN_V1;
        }

        return {minMixin, maxMixin};
      }

      /* This method is used by WalletService to determine if the mixin amount is correct
         for the current block height */
      static std::tuple<bool, std::string, std::error_code> validate(const uint32_t mixin, const uint32_t height)
      {
        auto [minMixin, maxMixin] = getMixinAllowableRange(height);

        std::stringstream str;

        if (mixin < minMixin)
        {
          str << "Mixin of " << mixin << " under minimum mixin threshold of " << minMixin;
          return {false, str.str(), make_error_code(CryptoNote::error::MIXIN_BELOW_THRESHOLD)};
        }
        else if (mixin > maxMixin)
        {
          str << "Mixin of " << mixin << " above maximum mixin threshold of " << maxMixin;
          return {false, str.str(), make_error_code(CryptoNote::error::MIXIN_ABOVE_THRESHOLD)};
        }

        return {true, std::string(), std::error_code()};
      }

      /* This method is commonly used by the node to determine if the transactions in the vector have
         the correct mixin (anonymity) as defined by the current rules */
      static std::tuple<bool, std::string> validate(std::vector<CachedTransaction> transactions, uint32_t height)
      {
        auto [minMixin, maxMixin] = getMixinAllowableRange(height);

        for (const auto& transaction : transactions)
        {
            auto [success, error] = validate(transaction, minMixin, maxMixin);

            if (!success)
            {
                return {false, error};
            }
        }

        return {true, std::string()};
      }

      /* This method is commonly used by the node to determine if the transaction has
         the correct mixin (anonymity) as defined by the current rules */
      static std::tuple<bool, std::string> validate(const CachedTransaction& transaction, uint64_t minMixin, uint64_t maxMixin)
      {
        uint64_t ringSize = 1;

        const auto tx = createTransaction(transaction.getTransaction());

        for (size_t i = 0; i < tx->getInputCount(); ++i) {
          if (tx->getInputType(i) != TransactionTypes::InputType::Key) {
            continue;
          }

          KeyInput input;
          tx->getInput(i, input);
          const uint64_t currentRingSize = input.outputIndexes.size();
          if (currentRingSize > ringSize) {
              ringSize = currentRingSize;
          }
        }

        /* Ring size = mixin + 1 - your transaction plus the others you mix with */
        const uint64_t mixin = ringSize - 1;

        std::stringstream str;

        if (mixin > maxMixin) {
          str << "Transaction " << transaction.getTransactionHash()
            << " is not valid. Reason: transaction mixin is too large (" << mixin
            << "). Maximum mixin allowed is " << maxMixin;

          return {false, str.str()};
        } else if (mixin < minMixin) {
          str << "Transaction " << transaction.getTransactionHash()
            << " is not valid. Reason: transaction mixin is too small (" << mixin
            << "). Minimum mixin allowed is " << minMixin;

          return {false, str.str()};
        }

        return {true, std::string()};
      }
  };
}