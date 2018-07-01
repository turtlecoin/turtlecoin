// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <stdexcept>
#include <algorithm>
#include <string>

#include "IWalletLegacy.h"

namespace CryptoNote
{
    class ISerializer;

    struct UnconfirmedTransferDetails;
    struct WalletLegacyTransaction;
    struct WalletLegacyTransfer;

    void serialize(UnconfirmedTransferDetails &utd, ISerializer &serializer);

    void serialize(WalletLegacyTransaction &txi, ISerializer &serializer);

    void serialize(WalletLegacyTransfer &tr, ISerializer &serializer);

}
