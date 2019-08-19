// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <config/CryptoNoteConfig.h>

/* Make sure everything in here is const - or it won't compile! */
namespace WalletConfig
{
    /* The prefix your coins address starts with */
    const std::string_view addressPrefix = "TRTL";

    /* Your coins 'Ticker', e.g. Monero = XMR, Bitcoin = BTC */
    const std::string ticker = "TRTL";

    /* The filename to output the CSV to in save_csv */
    const std::string csvFilename = "transactions.csv";

    /* The filename to read+write the address book to - consider starting with
       a leading '.' to make it hidden under mac+linux */
    const std::string addressBookFilename = ".addressBook.json";

    /* The name of your deamon */
    const std::string daemonName = "TurtleCoind";

    /* The name to call this wallet */
    const std::string walletName = "zedwallet";

    /* The name of service/walletd, the programmatic rpc interface to a
       wallet */
    const std::string walletdName = "turtle-service";

    /* The full name of your crypto */
    const std::string coinName = std::string(CryptoNote::CRYPTONOTE_NAME);

    /* Where can your users contact you for support? E.g. discord */
    const std::string contactLink = "http://chat.turtlecoin.lol";

    /* The number of decimals your coin has */
    const uint8_t numDecimalPlaces = CryptoNote::parameters::CRYPTONOTE_DISPLAY_DECIMAL_POINT;

    /* The length of a standard address for your coin */
    const uint16_t standardAddressLength = 99;

    /* The length of an integrated address for your coin - It's the same as
       a normal address, but there is a paymentID included in there - since
       payment ID's are 64 chars, and base58 encoding is done by encoding
       chunks of 8 chars at once into blocks of 11 chars, we can calculate
       this automatically */
    const uint16_t integratedAddressLength = standardAddressLength + ((64 * 11) / 8);

    /* The default fee value to use with transactions (in ATOMIC units!) */
    const uint64_t defaultFee = CryptoNote::parameters::MINIMUM_FEE;

    /* The minimum fee value to allow with transactions (in ATOMIC units!) */
    const uint64_t minimumFee = CryptoNote::parameters::MINIMUM_FEE;

    /* The minimum amount allowed to be sent - usually 1 (in ATOMIC units!) */
    const uint64_t minimumSend = 1;

    /* Is a mixin of zero disabled on your network? */
    const bool mixinZeroDisabled = true;

    /* If a mixin of zero is disabled, at what height was it disabled? E.g.
       fork height, or 0, if never allowed. This is ignored if a mixin of
       zero is allowed */
    const uint64_t mixinZeroDisabledHeight = CryptoNote::parameters::MIXIN_LIMITS_V2_HEIGHT;

    /**
     * Max size of a post body response - 10MB
     * Will decrease the amount of blocks requested from the daemon if this
     * is exceeded.
     * Note - blockStoreMemoryLimit - maxBodyResponseSize should be greater
     * than zero, or no data will get cached.
     * Further note: Currently blocks request are not decreased if this is
     * exceeded. Needs to be implemented in future?
     */
    const size_t maxBodyResponseSize = 1024 * 1024 * 10;

    /**
     * The amount of memory to use storing downloaded blocks - 50MB
     */
    const size_t blockStoreMemoryLimit = 1024 * 1024 * 50;
} // namespace WalletConfig
