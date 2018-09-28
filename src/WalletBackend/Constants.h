// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

namespace Constants
{
    /* We use this to check that the file is a wallet file, this bit does
       not get encrypted, and we can check if it exists before decrypting.
       If it isn't, it's not a wallet file. */
    const std::array<uint8_t, 64> IS_A_WALLET_IDENTIFIER =
    {
        0x49, 0x66, 0x20, 0x49, 0x20, 0x70, 0x75, 0x6c, 0x6c, 0x20, 0x74,
        0x68, 0x61, 0x74, 0x20, 0x6f, 0x66, 0x66, 0x2c, 0x20, 0x77, 0x69,
        0x6c, 0x6c, 0x20, 0x79, 0x6f, 0x75, 0x20, 0x64, 0x69, 0x65, 0x3f,
        0x0a, 0x49, 0x74, 0x20, 0x77, 0x6f, 0x75, 0x6c, 0x64, 0x20, 0x62,
        0x65, 0x20, 0x65, 0x78, 0x74, 0x72, 0x65, 0x6d, 0x65, 0x6c, 0x79,
        0x20, 0x70, 0x61, 0x69, 0x6e, 0x66, 0x75, 0x6c, 0x2e
    };

    /* We use this to check if the file has been correctly decoded, i.e.
       is the password correct. This gets encrypted into the file, and
       then when unencrypted the file should start with this - if it
       doesn't, the password is wrong */
    const std::array<uint8_t, 26> IS_CORRECT_PASSWORD_IDENTIFIER =
    {
        0x59, 0x6f, 0x75, 0x27, 0x72, 0x65, 0x20, 0x61, 0x20, 0x62, 0x69,
        0x67, 0x20, 0x67, 0x75, 0x79, 0x2e, 0x0a, 0x46, 0x6f, 0x72, 0x20,
        0x79, 0x6f, 0x75, 0x2e
    };

    /* The number of iterations of PBKDF2 to perform on the wallet
       password. */
    const uint64_t PBKDF2_ITERATIONS = 500000;

    /* What version of the file format are we on (to make it easier to
       upgrade the wallet format in the future) */
    const uint16_t WALLET_FILE_FORMAT_VERSION = 0;

    /* How large should the m_lastKnownBlockHashes container be */
    const uint32_t LAST_KNOWN_BLOCK_HASHES_SIZE = 100;

    /* Save a block hash checkpoint every BLOCK_HASH_CHECKPOINTS_INTERVAL
       blocks */
    const uint32_t BLOCK_HASH_CHECKPOINTS_INTERVAL = 5000;
}
