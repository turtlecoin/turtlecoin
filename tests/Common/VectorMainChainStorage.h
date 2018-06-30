// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <CryptoNoteCore/IMainChainStorage.h>
#include <CryptoNoteCore/Currency.h>

#include <vector>

namespace CryptoNote
{

    class VectorMainChainStorage : public IMainChainStorage
    {
    public:
        virtual void pushBlock(const RawBlock &rawBlock) override;

        virtual void popBlock() override;

        virtual RawBlock getBlockByIndex(uint32_t index) const override;

        virtual uint32_t getBlockCount() const override;

        virtual void clear() override;

    private:
        std::vector<RawBlock> storage;
    };

    std::unique_ptr<IMainChainStorage> createVectorMainChainStorage(const Currency &currency);

}
