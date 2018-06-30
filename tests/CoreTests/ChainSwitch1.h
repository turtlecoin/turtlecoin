// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "Chaingen.h"

/************************************************************************/
/*                                                                      */
/************************************************************************/
class gen_chain_switch_1 : public test_chain_unit_base
{
public:
    gen_chain_switch_1();

    bool generate(std::vector<test_event_entry> &events) const;

    bool check_split_not_switched(CryptoNote::Core &c, size_t ev_index, const std::vector<test_event_entry> &events);

    bool check_split_switched(CryptoNote::Core &c, size_t ev_index, const std::vector<test_event_entry> &events);

private:
    std::vector<CryptoNote::BlockTemplate> m_chain_1;

    CryptoNote::AccountBase m_recipient_account_1;
    CryptoNote::AccountBase m_recipient_account_2;
    CryptoNote::AccountBase m_recipient_account_3;
    CryptoNote::AccountBase m_recipient_account_4;

    std::vector<CryptoNote::Transaction> m_tx_pool;
};
