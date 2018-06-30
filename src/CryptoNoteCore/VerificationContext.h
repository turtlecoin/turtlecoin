// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once
namespace CryptoNote
{
    // TODO remove
    struct block_verification_context
    {
        bool m_added_to_main_chain;
        bool m_verifivation_failed; //bad block, should drop connection
        bool m_marked_as_orphaned;
        bool m_already_exists;
        bool m_switched_to_alt_chain;
    };
}
