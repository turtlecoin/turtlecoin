// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "gtest/gtest.h"
#include <Common/JsonValue.h>

using Common::JsonValue;

namespace
{

    std::vector<std::string> goodPatterns{
            "{}",
            "   {}   ",
            "   {   }   ",
            "100",
            "[10,20,30]",
            "  [  10  , \n 20  , \n  30  ]  ",
            "{\"prop\": 100}",
            "{\"prop\": 100, \"prop2\": [100, 20, 30] }",
            "{\"prop\": 100, \"prop2\": { \"p\":\"test\" } }",

    };


    std::vector<std::string> badPatterns{
            "",
            "1..2",
            "\n\n",
            "{",
            "[",
            "[100,",
            "[[]",
            "\"",
            "{\"prop: 100 }",
            "{\"prop\" 100 }",
            "{ prop: 100 }",
    };

}

TEST(JsonValue, testGoodPatterns)
{
    for (const auto &p : goodPatterns)
    {
        std::cout << "Pattern: " << p << std::endl;
        ASSERT_NO_THROW(Common::JsonValue::fromString(p));
    }
}

TEST(JsonValue, testBadPatterns)
{
    for (const auto &p : badPatterns)
    {
        ASSERT_ANY_THROW(Common::JsonValue::fromString(p));
    }
}

