// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>

namespace CryptoNote
{
    const uint32_t PORTABLE_STORAGE_SIGNATUREA = 0x01011101;

    const uint32_t PORTABLE_STORAGE_SIGNATUREB = 0x01020101; // bender's nightmare
    const uint8_t PORTABLE_STORAGE_FORMAT_VER = 1;

    const uint8_t PORTABLE_RAW_SIZE_MARK_MASK = 0x03;

    const uint8_t PORTABLE_RAW_SIZE_MARK_BYTE = 0;

    const uint8_t PORTABLE_RAW_SIZE_MARK_WORD = 1;

    const uint8_t PORTABLE_RAW_SIZE_MARK_DWORD = 2;

    const uint8_t PORTABLE_RAW_SIZE_MARK_INT64 = 3;

#ifndef MAX_STRING_LEN_POSSIBLE
#define MAX_STRING_LEN_POSSIBLE 2000000000 // do not let string be so big
#endif

    // data types

    const uint8_t BIN_KV_SERIALIZE_TYPE_INT64 = 1;

    const uint8_t BIN_KV_SERIALIZE_TYPE_INT32 = 2;

    const uint8_t BIN_KV_SERIALIZE_TYPE_INT16 = 3;

    const uint8_t BIN_KV_SERIALIZE_TYPE_INT8 = 4;

    const uint8_t BIN_KV_SERIALIZE_TYPE_UINT64 = 5;

    const uint8_t BIN_KV_SERIALIZE_TYPE_UINT32 = 6;

    const uint8_t BIN_KV_SERIALIZE_TYPE_UINT16 = 7;

    const uint8_t BIN_KV_SERIALIZE_TYPE_UINT8 = 8;

    const uint8_t BIN_KV_SERIALIZE_TYPE_DOUBLE = 9;

    const uint8_t BIN_KV_SERIALIZE_TYPE_STRING = 10;

    const uint8_t BIN_KV_SERIALIZE_TYPE_BOOL = 11;

    const uint8_t BIN_KV_SERIALIZE_TYPE_OBJECT = 12;

    const uint8_t BIN_KV_SERIALIZE_TYPE_ARRAY = 13;

    const uint8_t BIN_KV_SERIALIZE_FLAG_ARRAY = 0x80;

#pragma pack(push)
#pragma pack(1)
    struct KVBinaryStorageBlockHeader
    {
        uint32_t m_signature_a;
        uint32_t m_signature_b;
        uint8_t m_ver;
    };
#pragma pack(pop)

} // namespace CryptoNote
