// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoNoteCore/Account.h"
#include "CryptoNoteBoostSerialization.h"

//namespace CryptoNote {
namespace boost
{
    namespace serialization
    {
        template<class Archive>
        inline void serialize(Archive &a, CryptoNote::AccountKeys &x, const boost::serialization::version_type ver)
        {
            a & x.address;
            a & x.spendSecretKey;
            a & x.viewSecretKey;
        }

        template<class Archive>
        inline void
        serialize(Archive &a, CryptoNote::AccountPublicAddress &x, const boost::serialization::version_type ver)
        {
            a & x.spendPublicKey;
            a & x.viewPublicKey;
        }

    }
}
