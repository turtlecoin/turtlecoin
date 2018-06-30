// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoNoteBasic.h"
#include "crypto/chacha8.h"
#include "Serialization/ISerializer.h"
#include "crypto/crypto.h"

namespace Crypto
{

    bool serialize(PublicKey &pubKey, Common::StringView name, CryptoNote::ISerializer &serializer);

    bool serialize(SecretKey &secKey, Common::StringView name, CryptoNote::ISerializer &serializer);

    bool serialize(Hash &h, Common::StringView name, CryptoNote::ISerializer &serializer);

    bool serialize(chacha8_iv &chacha, Common::StringView name, CryptoNote::ISerializer &serializer);

    bool serialize(KeyImage &keyImage, Common::StringView name, CryptoNote::ISerializer &serializer);

    bool serialize(Signature &sig, Common::StringView name, CryptoNote::ISerializer &serializer);

    bool serialize(EllipticCurveScalar &ecScalar, Common::StringView name, CryptoNote::ISerializer &serializer);

    bool serialize(EllipticCurvePoint &ecPoint, Common::StringView name, CryptoNote::ISerializer &serializer);

}

namespace CryptoNote
{

    struct AccountKeys;
    struct TransactionExtraMergeMiningTag;

    enum class SerializationTag : uint8_t
    {
        Base = 0xff, Key = 0x2, Transaction = 0xcc, Block = 0xbb
    };

    void serialize(TransactionPrefix &txP, ISerializer &serializer);

    void serialize(Transaction & tx, ISerializer & serializer);

    void serialize(BaseTransaction &tx, ISerializer &serializer);

    void serialize(TransactionInput &in, ISerializer &serializer);

    void serialize(TransactionOutput &in, ISerializer &serializer);

    void serialize(BaseInput &gen, ISerializer &serializer);

    void serialize(KeyInput &key, ISerializer &serializer);

    void serialize(TransactionOutput &output, ISerializer &serializer);

    void serialize(TransactionOutputTarget &output, ISerializer &serializer);

    void serialize(KeyOutput &key, ISerializer &serializer);

    void serialize(BlockHeader &header, ISerializer &serializer);

    void serialize(BlockTemplate &block, ISerializer &serializer);

    void serialize(ParentBlockSerializer &pbs, ISerializer &serializer);

    void serialize(TransactionExtraMergeMiningTag &tag, ISerializer &serializer);

    void serialize(AccountPublicAddress &address, ISerializer &serializer);

    void serialize(AccountKeys &keys, ISerializer &s);

    void serialize(KeyPair &keyPair, ISerializer &serializer);

    void serialize(RawBlock &rawBlock, ISerializer &serializer);

}
