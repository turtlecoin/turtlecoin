// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <cstdint>

#include "CryptoNoteProtocol/ICryptoNoteProtocolObserver.h"
#include "CryptoNoteProtocol/ICryptoNoteProtocolQuery.h"
#include "CryptoNoteProtocol/CryptoNoteProtocolHandlerCommon.h"

class ICryptoNoteProtocolQueryStub : public CryptoNote::ICryptoNoteProtocolHandler
{
public:
    ICryptoNoteProtocolQueryStub() : peers(0), observedHeight(0), synchronized(false)
    {
    }

    virtual bool addObserver(CryptoNote::ICryptoNoteProtocolObserver *observer) override;

    virtual bool removeObserver(CryptoNote::ICryptoNoteProtocolObserver *observer) override;

    virtual uint32_t getObservedHeight() const override;

    virtual uint32_t getBlockchainHeight() const override;

    virtual size_t getPeerCount() const override;

    virtual bool isSynchronized() const override;

    void setPeerCount(uint32_t count);

    void setObservedHeight(uint32_t height);

    void setSynchronizedStatus(bool status);

    virtual void relayBlock(CryptoNote::NOTIFY_NEW_BLOCK_request &arg) override
    {};

    virtual void relayTransactions(const std::vector<CryptoNote::BinaryArray> &transactions) override
    {};


private:
    size_t peers;
    uint32_t observedHeight;
    uint32_t blockchainHeight;

    bool synchronized;
};
