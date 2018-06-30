// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <System/ErrorMessage.h>
#include <gtest/gtest.h>

using namespace System;

TEST(ErrorMessageTests, testErrorMessage)
{
    auto msg = errorMessage(100);
    ASSERT_EQ(msg.substr(0, 12), "result=100, ");
}
