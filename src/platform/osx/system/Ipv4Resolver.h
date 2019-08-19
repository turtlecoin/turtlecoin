// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <string>

namespace System
{
    class Dispatcher;

    class Ipv4Address;

    class Ipv4Resolver
    {
      public:
        Ipv4Resolver();

        explicit Ipv4Resolver(Dispatcher &dispatcher);

        Ipv4Resolver(const Ipv4Resolver &) = delete;

        Ipv4Resolver(Ipv4Resolver &&other);

        ~Ipv4Resolver();

        Ipv4Resolver &operator=(const Ipv4Resolver &) = delete;

        Ipv4Resolver &operator=(Ipv4Resolver &&other);

        Ipv4Address resolve(const std::string &host);

      private:
        Dispatcher *dispatcher;
    };

} // namespace System
