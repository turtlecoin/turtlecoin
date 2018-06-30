// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <list>
#include <vector>

#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "IWalletLegacy.h"
#include "ITransfersContainer.h"

namespace CryptoNote
{

    struct TxDustPolicy
    {
        uint64_t dustThreshold;
        bool addToFee;
        CryptoNote::AccountPublicAddress addrForDust;

        TxDustPolicy(uint64_t a_dust_threshold = 0, bool an_add_to_fee = true,
                     CryptoNote::AccountPublicAddress an_addr_for_dust = CryptoNote::AccountPublicAddress())
                : dustThreshold(a_dust_threshold), addToFee(an_add_to_fee), addrForDust(an_addr_for_dust)
        {}
    };

    struct SendTransactionContext
    {
        TransactionId transactionId;
        std::vector<CryptoNote::COMMAND_RPC_GET_RANDOM_OUTPUTS_FOR_AMOUNTS::outs_for_amount> outs;
        uint64_t foundMoney;
        std::list<TransactionOutputInformation> selectedTransfers;
        TxDustPolicy dustPolicy;
        uint64_t mixIn;
    };

} //namespace CryptoNote
