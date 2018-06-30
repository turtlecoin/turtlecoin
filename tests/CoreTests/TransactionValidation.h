// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "Chaingen.h"

struct get_tx_validation_base : public test_chain_unit_base
{
    get_tx_validation_base() : m_invalid_tx_index(0), m_invalid_block_index(0)
    {
        REGISTER_CALLBACK_METHOD(get_tx_validation_base, mark_invalid_tx);
        REGISTER_CALLBACK_METHOD(get_tx_validation_base, mark_invalid_block);
    }

    bool
    check_tx_verification_context(bool tve, bool tx_added, size_t event_idx, const CryptoNote::Transaction & /*tx*/)
    {
        return m_invalid_tx_index == event_idx ? !tve && !tx_added : tve && tx_added;
    }

    bool check_block_verification_context(std::error_code bve, size_t event_idx,
                                          const CryptoNote::BlockTemplate & /*block*/)
    {
        std::cout << "message: " << bve.message() << std::endl;
        return m_invalid_block_index == event_idx ? blockWasNotAdded(bve) : blockWasAdded(bve);
    }

    bool check_block_verification_context(std::error_code bve, size_t event_idx,
                                          const CryptoNote::RawBlock & /*block*/)
    {
        return check_block_verification_context(bve, event_idx, CryptoNote::BlockTemplate{});
    }

    bool
    mark_invalid_block(CryptoNote::Core & /*c*/, size_t ev_index, const std::vector<test_event_entry> & /*events*/)
    {
        m_invalid_block_index = ev_index + 1;
        return true;
    }

    bool mark_invalid_tx(CryptoNote::Core & /*c*/, size_t ev_index, const std::vector<test_event_entry> & /*events*/)
    {
        m_invalid_tx_index = ev_index + 1;
        return true;
    }

private:
    size_t m_invalid_tx_index;
    size_t m_invalid_block_index;
};

struct gen_tx_big_version : public get_tx_validation_base
{
    bool generate(std::vector<test_event_entry> &events) const;
};

struct gen_tx_unlock_time : public get_tx_validation_base
{
    bool generate(std::vector<test_event_entry> &events) const;
};

struct gen_tx_no_inputs_no_outputs : public get_tx_validation_base
{
    bool generate(std::vector<test_event_entry> &events) const;
};

struct gen_tx_no_inputs_has_outputs : public get_tx_validation_base
{
    bool generate(std::vector<test_event_entry> &events) const;
};

struct gen_tx_has_inputs_no_outputs : public get_tx_validation_base
{
    bool generate(std::vector<test_event_entry> &events) const;
};

struct gen_tx_invalid_input_amount : public get_tx_validation_base
{
    bool generate(std::vector<test_event_entry> &events) const;
};

struct gen_tx_in_to_key_wo_key_offsets : public get_tx_validation_base
{
    bool generate(std::vector<test_event_entry> &events) const;
};

struct gen_tx_key_offest_points_to_foreign_key : public get_tx_validation_base
{
    bool generate(std::vector<test_event_entry> &events) const;
};

struct gen_tx_sender_key_offest_not_exist : public get_tx_validation_base
{
    bool generate(std::vector<test_event_entry> &events) const;
};

struct gen_tx_mixed_key_offest_not_exist : public get_tx_validation_base
{
    bool generate(std::vector<test_event_entry> &events) const;
};

struct gen_tx_key_image_not_derive_from_tx_key : public get_tx_validation_base
{
    bool generate(std::vector<test_event_entry> &events) const;
};

struct gen_tx_key_image_is_invalid : public get_tx_validation_base
{
    bool generate(std::vector<test_event_entry> &events) const;
};

struct gen_tx_check_input_unlock_time : public get_tx_validation_base
{
    bool generate(std::vector<test_event_entry> &events) const;
};

struct gen_tx_txout_to_key_has_invalid_key : public get_tx_validation_base
{
    bool generate(std::vector<test_event_entry> &events) const;
};

struct gen_tx_output_with_zero_amount : public get_tx_validation_base
{
    bool generate(std::vector<test_event_entry> &events) const;
};

struct gen_tx_signatures_are_invalid : public get_tx_validation_base
{
    bool generate(std::vector<test_event_entry> &events) const;
};

struct GenerateTransactionWithZeroFee : public get_tx_validation_base
{
    bool generate(std::vector<test_event_entry> &events) const;
};
