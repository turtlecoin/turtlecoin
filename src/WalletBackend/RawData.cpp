// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

//////////////////////////////////
#include <WalletBackend/RawData.h>
//////////////////////////////////

/* Get a raw coinbase transaction from the transaction rubbish */
RawCoinbaseTransaction getRawCoinbaseTransaction(CryptoNote::Transaction t)
{
    RawCoinbaseTransaction transaction;

    /* Copy over the extra */
    transaction.extra = t.extra;

    /* Fill in the simplified key outputs */
    for (const auto &output : t.outputs)
    {
        KeyOutput keyOutput;

        keyOutput.amount = output.amount;
        keyOutput.key = boost::get<CryptoNote::KeyOutput>(output.target).key;

        transaction.keyOutputs.push_back(keyOutput);
    }

    return transaction;
}

/* Get a raw transaction from the transaction prefix rubbish */
RawTransaction getRawTransaction(CryptoNote::TransactionPrefix t)
{
    /* RawCoinbaseTransaction is a subset of RawTransaction, so we don't need
       to redo the work done in getRawCoinbaseTransaction() 
       FYI, TransactionPrefix == Transaction, minus some signatures */
    CryptoNote::Transaction tmp;

    /* Copy over the fields we need */
    tmp.extra = t.extra;
    tmp.outputs = t.outputs;

    /* Get the raw transaction */
    RawCoinbaseTransaction coinbase = getRawCoinbaseTransaction(tmp);

    RawTransaction transaction;

    /* Copy over the fields. Could use std::move here I guess */
    transaction.keyOutputs = coinbase.keyOutputs;
    transaction.extra = coinbase.extra;

    /* Fill in the missing field */
    for (const auto &input : t.inputs)
    {
        transaction.keyInputs.push_back(boost::get<CryptoNote::KeyInput>(input));
    }

    return transaction;
}
