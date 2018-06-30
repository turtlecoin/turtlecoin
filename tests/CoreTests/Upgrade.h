// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "Chaingen.h"

struct gen_upgrade : public test_chain_unit_base
{
    gen_upgrade();

    bool generate(std::vector<test_event_entry> &events) const;

    bool check_block_verification_context(std::error_code bve, size_t eventIdx, const CryptoNote::BlockTemplate &blk);

    bool markInvalidBlock(CryptoNote::Core &c, size_t evIndex, const std::vector<test_event_entry> &events);

    bool
    checkBlockTemplateVersionIsV1(CryptoNote::Core &c, size_t evIndex, const std::vector<test_event_entry> &events);

    bool
    checkBlockTemplateVersionIsV2(CryptoNote::Core &c, size_t evIndex, const std::vector<test_event_entry> &events);

    bool checkBlockRewardEqFee(CryptoNote::Core &c, size_t evIndex, const std::vector<test_event_entry> &events);

    bool checkBlockRewardIsZero(CryptoNote::Core &c, size_t evIndex, const std::vector<test_event_entry> &events);

    bool rememberCoinsInCirculationBeforeUpgrade(CryptoNote::Core &c, size_t evIndex,
                                                 const std::vector<test_event_entry> &events);

    bool rememberCoinsInCirculationAfterUpgrade(CryptoNote::Core &c, size_t evIndex,
                                                const std::vector<test_event_entry> &events);

private:
    bool checkBeforeUpgrade(std::vector<test_event_entry> &events, test_generator &generator,
                            const CryptoNote::BlockTemplate &parentBlock, const CryptoNote::AccountBase &minerAcc,
                            bool checkReward) const;

    bool checkAfterUpgrade(std::vector<test_event_entry> &events, test_generator &generator,
                           const CryptoNote::BlockTemplate &parentBlock, const CryptoNote::AccountBase &minerAcc) const;

    bool checkBlockTemplateVersion(CryptoNote::Core &c, uint8_t expectedMajorVersion, uint8_t expectedMinorVersion);

private:
    size_t m_invalidBlockIndex;
    size_t m_checkBlockTemplateVersionCallCounter;
    uint64_t m_coinsInCirculationBeforeUpgrade;
    uint64_t m_coinsInCirculationAfterUpgrade;
};
