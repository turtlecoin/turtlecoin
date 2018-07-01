// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <string>
#include <ostream>

namespace CryptoNote
{

    void
    importLegacyKeys(const std::string &legacyKeysFilename, const std::string &password, std::ostream &destination);

} //namespace CryptoNote
