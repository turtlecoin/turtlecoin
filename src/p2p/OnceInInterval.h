// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <time.h>

namespace CryptoNote
{
    class OnceInInterval
    {
      public:
        OnceInInterval(unsigned interval, bool startNow = true):
            m_interval(interval),
            m_lastCalled(startNow ? 0 : time(nullptr))
        {
        }

        template<class F> bool call(F func)
        {
            time_t currentTime = time(nullptr);

            if (currentTime - m_lastCalled > m_interval)
            {
                bool res = func();
                time(&m_lastCalled);
                return res;
            }

            return true;
        }

      private:
        time_t m_lastCalled;

        time_t m_interval;
    };

} // namespace CryptoNote
