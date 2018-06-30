// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "DoubleSpend.h"
#include "TestGenerator.h"

using namespace CryptoNote;

//======================================================================================================================

gen_double_spend_in_different_chains::gen_double_spend_in_different_chains()
{
    expected_blockchain_height = 5 + 2 * m_currency->minedMoneyUnlockWindow();

    REGISTER_CALLBACK_METHOD(gen_double_spend_in_different_chains, check_double_spend);
}

bool gen_double_spend_in_different_chains::generate(std::vector<test_event_entry> &events) const
{
    INIT_DOUBLE_SPEND_TEST();

    //SET_EVENT_VISITOR_SETT(events, event_visitor_settings::set_txs_keeped_by_block, true);
    MAKE_TX(events, tx_1, bob_account, alice_account, send_amount / 2 - m_currency->minimumFee(), blk_1);
    events.pop_back();
    MAKE_TX(events, tx_2, bob_account, alice_account, send_amount - m_currency->minimumFee(), blk_1);
    events.pop_back();

    // Main chain
    events.push_back(tx_1);
    MAKE_NEXT_BLOCK_TX1(events, blk_2, blk_1r, miner_account, tx_1);

    // Alternative chain
    //events.push_back(tx_2);
    MAKE_NEXT_BLOCK_TX1(events, blk_3, blk_1r, miner_account, tx_2);
    // Switch to alternative chain
    MAKE_NEXT_BLOCK(events, blk_4, blk_3, miner_account);
    CHECK_AND_NO_ASSERT_MES(expected_blockchain_height == CachedBlock(blk_4).getBlockIndex() + 1, false,
                            "expected_blockchain_height has invalid value");

    DO_CALLBACK(events, "check_double_spend");

    return true;
}

bool gen_double_spend_in_different_chains::check_double_spend(CryptoNote::Core &c, size_t /*ev_index*/,
                                                              const std::vector<test_event_entry> &events)
{
    DEFINE_TESTS_ERROR_CONTEXT("gen_double_spend_in_different_chains::check_double_spend");

    std::vector<BlockTemplate> blocks;
    auto rawBlocks = c.getBlocks(0, 100 + 2 * m_currency->minedMoneyUnlockWindow());
    blocks.resize(rawBlocks.size());
    for (size_t i = 0; i < rawBlocks.size(); ++i)
    {
        CHECK_TEST_CONDITION(fromBinaryArray(blocks[i], rawBlocks[i].block));
    }

    CHECK_EQ(expected_blockchain_height, blocks.size());

    CHECK_EQ(0, c.getPoolTransactionCount());
    CHECK_EQ(1, c.getAlternativeBlockCount());

    CryptoNote::AccountBase bob_account = boost::get<CryptoNote::AccountBase>(events[2]);
    CryptoNote::AccountBase alice_account = boost::get<CryptoNote::AccountBase>(events[3]);

    std::vector<CryptoNote::BlockTemplate> chain;
    map_hash2tx_t mtx;
    bool r = find_block_chain(events, chain, mtx, getBlockHash(blocks.back()));
    CHECK_TEST_CONDITION(r);
    CHECK_EQ(0, get_balance(bob_account, blocks, mtx));
    CHECK_EQ(send_amount - m_currency->minimumFee(), get_balance(alice_account, blocks, mtx));

    return true;
}
