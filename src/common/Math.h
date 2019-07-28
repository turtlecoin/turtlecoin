// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <algorithm>
#include <vector>

namespace Common
{
    template<class T> T medianValue(std::vector<T> &v)
    {
        if (v.empty())
        {
            return T();
        }

        if (v.size() == 1)
        {
            return v[0];
        }

        auto n = (v.size()) / 2;
        std::sort(v.begin(), v.end());
        // nth_element(v.begin(), v.begin()+n-1, v.end());
        if (v.size() % 2)
        { // 1, 3, 5...
            return v[n];
        }
        else
        { // 2, 4, 6...
            return (v[n - 1] + v[n]) / 2;
        }
    }

} // namespace Common
