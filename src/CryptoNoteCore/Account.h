// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "crypto/crypto.h"

namespace CryptoNote
{

    class ISerializer;

    /************************************************************************/
    /*                                                                      */
    /************************************************************************/
    class AccountBase
    {
    public:
        AccountBase();

        void generate();

        static void generateViewFromSpend(Crypto::SecretKey &, Crypto::SecretKey &, Crypto::PublicKey &);

        static void generateViewFromSpend(Crypto::SecretKey &, Crypto::SecretKey &);

        const AccountKeys &getAccountKeys() const;

        void setAccountKeys(const AccountKeys &keys);

        uint64_t get_createtime() const
        { return m_creation_timestamp; }

        void set_createtime(uint64_t val)
        { m_creation_timestamp = val; }

        void serialize(ISerializer &s);

        template<class t_archive>
        inline void serialize(t_archive &a, const unsigned int /*ver*/)
        {
            a & m_keys;
            a & m_creation_timestamp;
        }

    private:
        void setNull();

        AccountKeys m_keys;
        uint64_t m_creation_timestamp;
    };
}
