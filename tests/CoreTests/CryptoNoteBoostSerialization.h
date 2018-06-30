// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/variant.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/map.hpp>
#include <boost/foreach.hpp>
#include <boost/serialization/is_bitwise_serializable.hpp>
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "UnorderedContainersBoostSerialization.h"
#include "crypto/crypto.h"

//namespace CryptoNote {
namespace boost
{
    namespace serialization
    {

        //---------------------------------------------------
        template<class Archive>
        inline void serialize(Archive &a, Crypto::PublicKey &x, const boost::serialization::version_type ver)
        {
            a & reinterpret_cast<char (&)[sizeof(Crypto::PublicKey)]>(x);
        }

        template<class Archive>
        inline void serialize(Archive &a, Crypto::SecretKey &x, const boost::serialization::version_type ver)
        {
            a & reinterpret_cast<char (&)[sizeof(Crypto::SecretKey)]>(x);
        }

        template<class Archive>
        inline void serialize(Archive &a, Crypto::KeyDerivation &x, const boost::serialization::version_type ver)
        {
            a & reinterpret_cast<char (&)[sizeof(Crypto::KeyDerivation)]>(x);
        }

        template<class Archive>
        inline void serialize(Archive &a, Crypto::KeyImage &x, const boost::serialization::version_type ver)
        {
            a & reinterpret_cast<char (&)[sizeof(Crypto::KeyImage)]>(x);
        }

        template<class Archive>
        inline void serialize(Archive &a, Crypto::Signature &x, const boost::serialization::version_type ver)
        {
            a & reinterpret_cast<char (&)[sizeof(Crypto::Signature)]>(x);
        }

        template<class Archive>
        inline void serialize(Archive &a, Crypto::Hash &x, const boost::serialization::version_type ver)
        {
            a & reinterpret_cast<char (&)[sizeof(Crypto::Hash)]>(x);
        }

        template<class Archive>
        inline void serialize(Archive &a, CryptoNote::KeyOutput &x, const boost::serialization::version_type ver)
        {
            a & x.key;
        }

        template<class Archive>
        inline void serialize(Archive &a, CryptoNote::BaseInput &x, const boost::serialization::version_type ver)
        {
            a & x.blockIndex;
        }

        template<class Archive>
        inline void serialize(Archive &a, CryptoNote::KeyInput &x, const boost::serialization::version_type ver)
        {
            a & x.amount;
            a & x.outputIndexes;
            a & x.keyImage;
        }

        template<class Archive>
        inline void
        serialize(Archive &a, CryptoNote::TransactionOutput &x, const boost::serialization::version_type ver)
        {
            a & x.amount;
            a & x.target;
        }


        template<class Archive>
        inline void serialize(Archive &a, CryptoNote::Transaction &x, const boost::serialization::version_type ver)
        {
            a & x.version;
            a & x.unlockTime;
            a & x.inputs;
            a & x.outputs;
            a & x.extra;
            a & x.signatures;
        }


        template<class Archive>
        inline void serialize(Archive &a, CryptoNote::BlockTemplate &b, const boost::serialization::version_type ver)
        {
            std::cout << "boooooooooooost serialize" << std::endl;
            a & b.majorVersion;
            a & b.minorVersion;
            a & b.timestamp;
            a & b.previousBlockHash;
            a & b.nonce;
            //------------------
            a & b.baseTransaction;
            a & b.transactionHashes;
        }
    }
}

//}
