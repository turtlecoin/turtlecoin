// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <list>
#include <memory>

namespace CryptoNote
{
    class P2pContext;

    class P2pContextOwner
    {
      public:
        typedef std::list<std::unique_ptr<P2pContext>> ContextList;

        P2pContextOwner(P2pContext *ctx, ContextList &contextList);

        P2pContextOwner(P2pContextOwner &&other);

        P2pContextOwner(const P2pContextOwner &other) = delete;

        ~P2pContextOwner();

        P2pContext &get();

        P2pContext *operator->();

      private:
        ContextList &contextList;

        ContextList::iterator contextIterator;
    };

} // namespace CryptoNote
