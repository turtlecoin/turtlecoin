// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.
#pragma once

#include "common/ObserverManager.h"

namespace CryptoNote
{
    template<typename Observer, typename Base> class IObservableImpl : public Base
    {
      public:
        virtual void addObserver(Observer *observer) override
        {
            m_observerManager.add(observer);
        }

        virtual void removeObserver(Observer *observer) override
        {
            m_observerManager.remove(observer);
        }

      protected:
        Tools::ObserverManager<Observer> m_observerManager;
    };

} // namespace CryptoNote
