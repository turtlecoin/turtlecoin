// Copyright (c) 2018-2019, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

//////////////////////////
#include <Errors/Errors.h>
//////////////////////////

#include <ostream>

std::string Error::getErrorMessage() const
{
    /* Custom message being used, return that instead */
    if (m_customMessage != "")
    {
        return m_customMessage;
    }

    switch (m_errorCode)
    {
        case SUCCESS:
        {
            return "The operation completed successfully.";
        }
        case FILENAME_NON_EXISTENT:
        {
            return "The filename you are attempting to open does not exist, "
                   "or the wallet does not have permission to open it.";
        }
        case INVALID_WALLET_FILENAME:
        {
            return "We could not open/save to the filename given. Possibly "
                   "invalid characters, or permission issues.";
        }
        case NOT_A_WALLET_FILE:
        {
            return "This file is not a wallet file, or is not a wallet file "
                   "type supported by this wallet version.";
        }
        case WALLET_FILE_CORRUPTED:
        {
            return "This wallet file appears to have gotten corrupted.";
        }
        case WRONG_PASSWORD:
        {
            return "The password given for this wallet is incorrect.";
        }
        case UNSUPPORTED_WALLET_FILE_FORMAT_VERSION:
        {
            return "This wallet file appears to be from a newer or older "
                   "version of the software, that we do not support.";
        }
        case INVALID_MNEMONIC:
        {
            return "The mnemonic seed given is invalid.";
        }
        case WALLET_FILE_ALREADY_EXISTS:
        {
            return "The wallet file you are attempting to create already "
                   "exists. Please delete it first.";
        }
        case ADDRESS_NOT_IN_WALLET:
        {
            return "The address given does not exist in the wallet container, "
                   "but is required to exist for this operation.";
        }
        case NOT_ENOUGH_BALANCE:
        {
            return "Not enough unlocked funds were found to cover this "
                   "transaction in the subwallets specified (or all wallets, "
                   "if not specified. (Sum of amounts + fee + node fee)";
        }
        case ADDRESS_WRONG_LENGTH:
        {
            return "The address given is too short or too long.";
        }
        case ADDRESS_WRONG_PREFIX:
        {
            return "The address does not have the correct prefix corresponding "
                   "to this coin - it appears to be an address for another "
                   "cryptocurrency.";
        }
        case ADDRESS_NOT_BASE58:
        {
            return "The address contains invalid characters, that are not in "
                   "the base58 set.";
        }
        case ADDRESS_NOT_VALID:
        {
            return "The address given is not valid. Possibly invalid checksum. "
                   "Most likely a typo.";
        }
        case INTEGRATED_ADDRESS_PAYMENT_ID_INVALID:
        {
            return "The payment ID stored in the integrated address supplied "
                   "is not valid.";
        }
        case FEE_TOO_SMALL:
        {
            return "The fee given for this transaction is below the minimum "
                   "allowed network fee.";
        }
        case NO_DESTINATIONS_GIVEN:
        {
            return "The destinations array (amounts/addresses) is empty.";
        }
        case AMOUNT_IS_ZERO:
        {
            return "One of the destination parameters has an amount given of "
                   "zero.";
        }
        case FAILED_TO_CREATE_RING_SIGNATURE:
        {
            return "Failed to create ring signature - probably a programmer "
                   "error, or a corrupted wallet.";
        }
        case MIXIN_TOO_SMALL:
        {
            return "The mixin value given is too low to be accepted by the "
                   "network (based on the current height known by the wallet)";
        }
        case MIXIN_TOO_BIG:
        {
            return "The mixin value given is too high to be accepted by the "
                   "network (based on the current height known by the wallet)";
        }
        case PAYMENT_ID_WRONG_LENGTH:
        {
            return "The payment ID given is not 64 characters long.";
        }
        case PAYMENT_ID_INVALID:
        {
            return "The payment ID given is not a hex string (A-Za-z0-9)";
        }
        case ADDRESS_IS_INTEGRATED:
        {
            return "The address given is an integrated address, but integrated "
                   "addresses aren't valid for this parameter, for example, "
                   "change address.";
        }
        case CONFLICTING_PAYMENT_IDS:
        {
            return "Conflicting payment IDs were given. This could mean "
                   "an integrated address + payment ID were given, where "
                   "they are not the same, or that multiple integrated "
                   "addresses with different payment IDs were given.";
        }
        case CANT_GET_FAKE_OUTPUTS:
        {
            return "Failed to get fake outputs from the daemon to obscure "
                   "our transaction, and mixin is not zero.";
        }
        case NOT_ENOUGH_FAKE_OUTPUTS:
        {
            return "We could not get enough fake outputs for this transaction "
                   "to complete. If possible, try lowering the mixin value "
                   "used, or decrease the amount you are sending.";
        }
        case INVALID_GENERATED_KEYIMAGE:
        {
            return "The key image we generated is invalid - probably a "
                   "programmer error, or a corrupted wallet.";
        }
        case DAEMON_OFFLINE:
        {
            return "We were not able to submit our request to the daemon. "
                   "Ensure it is online and not frozen.";
        }
        case DAEMON_ERROR:
        {
            return "An error occured whilst the daemon processed the request. "
                   "Possibly our software is outdated, the daemon is faulty, "
                   "or there is a programmer error. Check your daemon logs "
                   "for more info. (set_log 4)";
        }
        case TOO_MANY_INPUTS_TO_FIT_IN_BLOCK:
        {
            return "The transaction is too large (in BYTES, not AMOUNT) to fit "
                   "in a block. Either decrease the amount you are sending, "
                   "perform fusion transactions, or decrease mixin (if possible).";
        }
        case MNEMONIC_INVALID_WORD:
        {
            return "The mnemonic seed given has a word that is not present in "
                   "the english word list.";
        }
        case MNEMONIC_WRONG_LENGTH:
        {
            return "The mnemonic seed given is the wrong length.";
        }
        case MNEMONIC_INVALID_CHECKSUM:
        {
            return "The mnemonic seed given has an invalid checksum word.";
        }
        case FULLY_OPTIMIZED:
        {
            return "Cannot send fusion transaction - wallet is already fully optimized.";
        }
        case FUSION_MIXIN_TOO_LARGE:
        {
            return "Cannot send fusion transacton - mixin is too large to meet "
                   "input/output ratio requirements whilst remaining in "
                   "size constraints.";
        }
        case SUBWALLET_ALREADY_EXISTS:
        {
            return "A subwallet with the given key already exists.";
        }
        case ILLEGAL_VIEW_WALLET_OPERATION:
        {
            return "This function cannot be called when using a view wallet.";
        }
        case ILLEGAL_NON_VIEW_WALLET_OPERATION:
        {
            return "This function can only be used when using a view wallet.";
        }
        case WILL_OVERFLOW:
        {
            return "This operation will cause integer overflow. Please decrease "
                   "the amounts you are sending.";
        }
        case KEYS_NOT_DETERMINISTIC:
        {
            return "You cannot get a mnemonic seed for this address, as the "
                   "view key is derived in terms of the spend key.";
        }
        case CANNOT_DELETE_PRIMARY_ADDRESS:
        {
            return "Each wallet has a primary address when created, this address "
                   "cannot be removed.";
        }
        case TX_PRIVATE_KEY_NOT_FOUND:
        {
            return "Couldn't find the private key for this transaction. The "
                   "transaction must exist, and have been sent by this program. "
                   "Transaction private keys cannot be found upon rescanning/"
                   "reimporting.";
        }
        case AMOUNTS_NOT_PRETTY:
        {
            return "The created transaction isn't comprised of only 'Pretty' "
                   "amounts. This will cause the outputs to be unmixable. "
                   "Almost certainly a programmer error. Cancelling transaction.";
        }
        case UNEXPECTED_FEE:
        {
            return "The fee of the created transaction is not the same as that "
                   "which was specified (0 for fusion transactions). Almost "
                   "certainly a programmer error. Cancelling transaction.";
        }
        case NEGATIVE_VALUE_GIVEN:
        {
            return "The input for this operation must be greater than or "
                   "equal to zero, but a negative number was given.";
        }
        case INVALID_KEY_FORMAT:
        {
            return "The public/private key or hash given is not a 64 char "
                   "hex string.";
        }
        case HASH_WRONG_LENGTH:
        {
            return "The hash given is not 64 characters long.";
        }
        case HASH_INVALID:
        {
            return "The hash given is not a hex string (A-Za-z0-9)";
        }
        case NON_INTEGER_GIVEN:
        {
            return "The number given is a float, not an integer.";
        }
        case INVALID_PUBLIC_KEY:
        {
            return "The public key given is not a valid ed25519 public key.";
        }
        case INVALID_PRIVATE_KEY:
        {
            return "The private key given is not a valid ed25519 public key.";
        }
        /* No default case so the compiler warns us if we missed one */
    }
}

ErrorCode Error::getErrorCode() const
{
    return m_errorCode;
}
