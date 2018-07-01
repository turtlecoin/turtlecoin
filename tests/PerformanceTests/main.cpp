// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "PerformanceTests.h"
#include "PerformanceUtils.h"

// tests
#include "ConstructTransaction.h"
#include "CheckRingSignature.h"
#include "CryptoNoteSlowHash.h"
#include "DerivePublicKey.h"
#include "DeriveSecretKey.h"
#include "GenerateKeyDerivation.h"
#include "GenerateKeyImage.h"
#include "GenerateKeyImageHelper.h"
#include "IsOutToAccount.h"

int main(int argc, char** argv)
{
  set_process_affinity(1);
  set_thread_high_priority();

  performance_timer timer;
  timer.start();

  TEST_PERFORMANCE2(test_construct_tx, 1, 1);
  TEST_PERFORMANCE2(test_construct_tx, 1, 2);
  TEST_PERFORMANCE2(test_construct_tx, 1, 10);
  TEST_PERFORMANCE2(test_construct_tx, 1, 100);
  TEST_PERFORMANCE2(test_construct_tx, 1, 1000);

  TEST_PERFORMANCE2(test_construct_tx, 2, 1);
  TEST_PERFORMANCE2(test_construct_tx, 2, 2);
  TEST_PERFORMANCE2(test_construct_tx, 2, 10);
  TEST_PERFORMANCE2(test_construct_tx, 2, 100);

  TEST_PERFORMANCE2(test_construct_tx, 10, 1);
  TEST_PERFORMANCE2(test_construct_tx, 10, 2);
  TEST_PERFORMANCE2(test_construct_tx, 10, 10);
  TEST_PERFORMANCE2(test_construct_tx, 10, 100);

  TEST_PERFORMANCE2(test_construct_tx, 100, 1);
  TEST_PERFORMANCE2(test_construct_tx, 100, 2);
  TEST_PERFORMANCE2(test_construct_tx, 100, 10);
  TEST_PERFORMANCE2(test_construct_tx, 100, 100);

  TEST_PERFORMANCE1(test_check_ring_signature, 1);
  TEST_PERFORMANCE1(test_check_ring_signature, 2);
  TEST_PERFORMANCE1(test_check_ring_signature, 10);
  TEST_PERFORMANCE1(test_check_ring_signature, 100);

  TEST_PERFORMANCE0(test_is_out_to_acc);
  TEST_PERFORMANCE0(test_generate_key_image_helper);
  TEST_PERFORMANCE0(test_generate_key_derivation);
  TEST_PERFORMANCE0(test_generate_key_image);
  TEST_PERFORMANCE0(test_derive_public_key);
  TEST_PERFORMANCE0(test_derive_secret_key);

  TEST_PERFORMANCE0(test_cn_slow_hash);

  std::cout << "Tests finished. Elapsed time: " << timer.elapsed_ms() / 1000 << " sec" << std::endl;

  return 0;
}
