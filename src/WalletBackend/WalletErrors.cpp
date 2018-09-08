// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

///////////////////////////////////////
#include <WalletBackend/WalletErrors.h>
///////////////////////////////////////

/* TODO: Fill me in */
std::string getErrorMessage(WalletError error)
{
    switch (error)
    {
        default:
        {
            return "Unknown error - Error code: " + std::to_string(error);
        }
    }
}
