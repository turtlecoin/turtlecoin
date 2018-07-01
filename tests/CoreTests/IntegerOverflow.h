// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once 
#include "Chaingen.h"

struct gen_uint_overflow_base : public test_chain_unit_base {
  gen_uint_overflow_base();

  bool check_tx_verification_context(bool tve, bool tx_added, size_t event_idx, const CryptoNote::Transaction& tx);
  bool check_block_verification_context(std::error_code bve, size_t event_idx, const CryptoNote::BlockTemplate& block);
  bool check_block_verification_context(std::error_code bve, size_t event_idx, const CryptoNote::RawBlock& block);

  bool mark_last_valid_block(CryptoNote::Core& c, size_t ev_index, const std::vector<test_event_entry>& events);

private:
  size_t m_last_valid_block_event_idx;
};

struct gen_uint_overflow_1 : public gen_uint_overflow_base {
  bool generate(std::vector<test_event_entry>& events) const;
};

struct gen_uint_overflow_2 : public gen_uint_overflow_base {
  bool generate(std::vector<test_event_entry>& events) const;
};
