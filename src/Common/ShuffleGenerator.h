// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <unordered_map>
#include <random>

class SequenceEnded : public std::runtime_error
{
public:
    SequenceEnded() : std::runtime_error("shuffle sequence ended")
    {
    }

    ~SequenceEnded()
    {}
};

template<typename T, typename Gen>
class ShuffleGenerator
{
public:

    ShuffleGenerator(T n, const Gen &gen = Gen()) :
            N(n), generator(gen), count(n)
    {}

    T operator()()
    {

        if (count == 0)
        {
            throw SequenceEnded();
        }

        typedef typename std::uniform_int_distribution<T> distr_t;
        typedef typename distr_t::param_type param_t;

        distr_t distr;

        T value = distr(generator, param_t(0, --count));

        auto rvalIt = selected.find(count);
        auto rval = rvalIt != selected.end() ? rvalIt->second : count;

        auto lvalIt = selected.find(value);

        if (lvalIt != selected.end())
        {
            value = lvalIt->second;
            lvalIt->second = rval;
        } else
        {
            selected[value] = rval;
        }

        return value;
    }

    bool empty() const
    {
        return count == 0;
    }

    void reset()
    {
        count = N;
        selected.clear();
    }

private:

    std::unordered_map<T, T> selected;
    T count;
    const T N;
    Gen generator;
};
