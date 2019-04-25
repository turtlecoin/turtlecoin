// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <string>

/* Note: Putting the number of the error is not needed, as they auto increment,
   however, it makes it easier to see at a glance what error you got, whilst
   developing */
enum ErrorCode
{
    /* No error, operation suceeded. */
    SUCCESS = 0,

    /* The wallet filename given does not exist or the program does not have
       permission to view it */
    FILENAME_NON_EXISTENT = 1,

    /* The output filename was unable to be opened for saving, probably due
       to invalid characters */
    INVALID_WALLET_FILENAME = 2,

    /* The wallet does not have the wallet identifier prefix */
    NOT_A_WALLET_FILE = 3,

    /* The file has the correct wallet file prefix, but is corrupted in some
       other way, such as a missing IV */
    WALLET_FILE_CORRUPTED = 4,

    /* Either the AES decryption failed due to wrong padding, or the decrypted
       data does not have the correct prefix indicating the password is
       correct. */
    WRONG_PASSWORD = 5,

    /* The wallet file is using a different version than the version supported
       by this version of the software. (Also could be potential corruption.) */
    UNSUPPORTED_WALLET_FILE_FORMAT_VERSION = 6,

    /* The mnemonic seed is invalid for some reason, for example, it has the
       wrong length, or an invalid checksum */
    INVALID_MNEMONIC = 7,

    /* Trying to create a wallet file which already exists */
    WALLET_FILE_ALREADY_EXISTS = 8,

    /* Operation will cause int overflow */
    WILL_OVERFLOW = 9,

    /* The address given does not exist in this container, and it's required,
       for example you specified it as the address to get the balance from */
    ADDRESS_NOT_IN_WALLET = 10,

    /* Amount + fee is greater than the total balance available in the
       subwallets specified (or all wallets, if not specified) */
    NOT_ENOUGH_BALANCE = 11,

    /* The address is the wrong length - neither a standard, nor an integrated
       address */
    ADDRESS_WRONG_LENGTH = 12,

    /* The address does not have the correct prefix, e.g. does not begin with
       TRTL (or whatever is specified in WalletConfig::addressPrefix) */
    ADDRESS_WRONG_PREFIX = 13,

    /* The address is not fully comprised of base58 characters */
    ADDRESS_NOT_BASE58 = 14,

    /* The address is invalid for some other reason (possibly checksum) */
    ADDRESS_NOT_VALID = 15,

    /* The payment ID encoded in the integrated address is not valid */ 
    INTEGRATED_ADDRESS_PAYMENT_ID_INVALID = 16,

    /* The fee given is lower than the CryptoNote::parameters::MINIMUM_FEE */
    FEE_TOO_SMALL = 17,

    /* The destinations array is empty */
    NO_DESTINATIONS_GIVEN = 18,

    /* One of the destination parameters has an amount given of zero. */
    AMOUNT_IS_ZERO = 19,

    /* Something went wrong creating the ring signatures. Probably a programmer
       error */
    FAILED_TO_CREATE_RING_SIGNATURE = 20,

    /* The mixin given is too low for the current height known by the wallet */
    MIXIN_TOO_SMALL = 21,

    /* The mixin given is too large for the current height known by the wallet */
    MIXIN_TOO_BIG = 22,

    /* Payment ID is not 64 chars */
    PAYMENT_ID_WRONG_LENGTH = 23,

    /* The payment ID is not hex */
    PAYMENT_ID_INVALID = 24,

    /* The address is an integrated address - but integrated addresses aren't
       valid for this parameter, for example, change address */
    ADDRESS_IS_INTEGRATED = 25,

    /* Conflicting payment ID's were found, due to integrated addresses. These
       could mean an integrated address + payment ID were given, where they
       are not the same, or that multiple integrated addresses with different
       payment IDs were given */
    CONFLICTING_PAYMENT_IDS = 26,

    /* Can't get mixin/fake outputs from the daemon, and mixin is not zero */
    CANT_GET_FAKE_OUTPUTS = 27,

    /* We got mixin/fake outputs from the daemon, but not enough. E.g. using a
       mixin of 3, we only got one fake output -> can't form transaction.
       This is most likely to be encountered on new networks, where not
       enough outputs have been created, or if you have a very large output
       that not enough have been created of.

       Try resending the transaction with a mixin of zero, if that is an option
       on your network. */
    NOT_ENOUGH_FAKE_OUTPUTS = 28,

    /* The key image generated was not valid. This is most likely a programmer
       error. */
    INVALID_GENERATED_KEYIMAGE = 29,

    /* Could not contact the daemon to complete the request. Ensure it is
       online and not frozen */
    DAEMON_OFFLINE = 30,

    /* An error occured whilst the daemon processed the request. Possibly our
       software is outdated, the daemon is faulty, or there is a programmer
       error. Check your daemon logs for more info (set_log 4) */
    DAEMON_ERROR = 31,

    /* The transction is too large (in BYTES, not AMOUNT) to fit in a block.
       Either:
       1) decrease the amount you are sending
       2) decrease the mixin value
       3) split your transaction up into multiple smaller transactions
       4) perform fusion transaction to combine multiple small inputs into
          fewer, larger inputs. */
    TOO_MANY_INPUTS_TO_FIT_IN_BLOCK = 32,

    /* Mnemonic has a word that is not in the english word list */
    MNEMONIC_INVALID_WORD = 33,

    /* Mnemonic seed is not 25 words */
    MNEMONIC_WRONG_LENGTH = 34,

    /* The mnemonic seed has an invalid checksum word */
    MNEMONIC_INVALID_CHECKSUM = 35,

    /* Don't have enough inputs to make a fusion transaction, wallet is fully
       optimized */
    FULLY_OPTIMIZED = 36,

    /* Mixin given for this fusion transaction is too large to be able to hit
       the min input requirement */
    FUSION_MIXIN_TOO_LARGE = 37,

    /* Attempted to add a subwallet which already exists in the container */
    SUBWALLET_ALREADY_EXISTS = 38,

    /* Cannot perform this operation when using a view wallet */
    ILLEGAL_VIEW_WALLET_OPERATION = 39,

    /* Cannot perform this operation when using a non view wallet */
    ILLEGAL_NON_VIEW_WALLET_OPERATION = 40,

    /* View key is not derived from spend key for this address */
    KEYS_NOT_DETERMINISTIC = 41,

    /* The primary address cannot be deleted */
    CANNOT_DELETE_PRIMARY_ADDRESS = 42,

    /* Couldn't find the private key for this hash */
    TX_PRIVATE_KEY_NOT_FOUND = 43,

    /* Amounts not a member of PRETTY_AMOUNTS */
    AMOUNTS_NOT_PRETTY = 44,

    /* Tx fee is not the same as specified fee */
    UNEXPECTED_FEE = 45,

    /* Value given is negative, but must be >= 0
       NOTE: Not used in WalletBackend, only here to maintain API compatibility
       with turtlecoin-wallet-backend-js */
    NEGATIVE_VALUE_GIVEN = 46,

    /* Key is not 64 char hex 
       NOTE: Not used in WalletBackend, only here to maintain API compatibility
       with turtlecoin-wallet-backend-js */
    INVALID_KEY_FORMAT = 47,

    /* Hash not 64 chars */
    HASH_WRONG_LENGTH = 48,

    /* Hash not hex */
    HASH_INVALID = 49,

    /* Number is a float, not an integer
       NOTE: Not used in WalletBackend, only here to maintain API compatibility
       with turtlecoin-wallet-backend-js */
    NON_INTEGER_GIVEN = 50,

    /* Not on ed25519 curve */
    INVALID_PUBLIC_KEY = 51,

    /* Not on ed25519 curve */
    INVALID_PRIVATE_KEY = 52,
};

class Error
{
    public:
        /* Default constructor */
        Error() : m_errorCode(SUCCESS) {};

        Error(const ErrorCode code) :
            m_errorCode(code) {};

        /* We can use a custom message instead of our standard message, for example,
           if the message depends upon the parameters. E.g: "Mnemonic seed should
           be 25 words, but it is 23 words" */
        Error(
            const ErrorCode code,
            const std::string customMessage) :
            m_errorCode(code),
            m_customMessage(customMessage) {};

        std::string getErrorMessage() const;

        ErrorCode getErrorCode() const;

        bool operator==(const ErrorCode code) const
        {
            return code == m_errorCode;
        }

        bool operator!=(const ErrorCode code) const
        {
            return !(code == m_errorCode);
        }

        /* Allows us to do stuff like:
           if (error) {}
           Returns true if the error code is not success. */
        explicit operator bool() const
        {
            return m_errorCode != SUCCESS;
        }

    private:
        /* May be empty */
        std::string m_customMessage;

        ErrorCode m_errorCode;
};

/* Overloading the << operator */
inline std::ostream &operator<<(std::ostream &os, const Error &error)
{
    os << error.getErrorMessage();
    return os;
}
