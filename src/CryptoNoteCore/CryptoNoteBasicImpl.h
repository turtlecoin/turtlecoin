// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "Common/StringTools.h"
#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"


namespace CryptoNote
{
    /************************************************************************/
    /*                                                                      */
    /************************************************************************/
    template<class t_array>
    struct array_hasher : std::unary_function<t_array &, size_t>
    {
        size_t operator()(const t_array &val) const
        {
            return boost::hash_range(&val.data[0], &val.data[sizeof(val.data)]);
        }
    };

    /************************************************************************/
    /* CryptoNote helper functions                                          */
    /************************************************************************/
    uint64_t getPenalizedAmount(uint64_t amount, size_t medianSize, size_t currentBlockSize);

    std::string getAccountAddressAsStr(uint64_t prefix, const AccountPublicAddress &adr);

    bool parseAccountAddressString(uint64_t &prefix, AccountPublicAddress &adr, const std::string &str);

    bool is_coinbase(const Transaction &tx);

    //bool operator ==(const CryptoNote::Transaction& a, const CryptoNote::Transaction& b);
    //bool operator ==(const CryptoNote::Block& a, const CryptoNote::Block& b);
}

template<class T>
std::ostream &print256(std::ostream &o, const T &v)
{
    return o << Common::podToHex(v);
}

bool parse_hash256(const std::string &str_hash, Crypto::Hash &hash);

namespace Crypto
{
    inline std::ostream &operator<<(std::ostream &o, const Crypto::PublicKey &v)
    { return print256(o, v); }

    inline std::ostream &operator<<(std::ostream &o, const Crypto::SecretKey &v)
    { return print256(o, v); }

    inline std::ostream &operator<<(std::ostream &o, const Crypto::KeyDerivation &v)
    { return print256(o, v); }

    inline std::ostream &operator<<(std::ostream &o, const Crypto::KeyImage &v)
    { return print256(o, v); }

    inline std::ostream &operator<<(std::ostream &o, const Crypto::Signature &v)
    { return print256(o, v); }

    inline std::ostream &operator<<(std::ostream &o, const Crypto::Hash &v)
    { return print256(o, v); }
}
