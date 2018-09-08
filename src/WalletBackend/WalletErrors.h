enum WalletError
{
    /* No error, operation suceeded. */
    SUCCESS = 0,

    /* The wallet filename given does not exist or the program does not have
       permission to view it */
    FILENAME_NON_EXISTENT = 1,

    /* The output filename was unable to be opened for saving */
    FAILED_TO_SAVE_WALLET,

    /* The wallet does not have the wallet identifier prefix */
    NOT_A_WALLET_FILE,

    /* The file has the correct wallet file prefix, but is corrupted in some
       other way, such as a missing IV */
    WALLET_FILE_CORRUPTED,

    /* Either the AES decryption failed due to wrong padding, or the decrypted
       data does not have the correct prefix indicating the password is
       correct. */
    WRONG_PASSWORD,
};
