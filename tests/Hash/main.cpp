// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <cstddef>
#include <fstream>
#include <iomanip>
#include <ios>
#include <string>

#include "crypto/hash.h"
#include "../Io.h"

using namespace std;
typedef Crypto::Hash chash;

Crypto::cn_context *context;

extern "C" {
#ifdef _MSC_VER
#pragma warning(disable: 4297)
#endif

  static void hash_tree(const void *data, size_t length, char *hash) {
    if ((length & 31) != 0) {
      throw ios_base::failure("Invalid input length for tree_hash");
    }
    Crypto::tree_hash((const char (*)[32]) data, length >> 5, hash);
  }

  static void cn_v6(const void *data, size_t length, char *hash) {
    cn_slow_hash_v6(*context, data, length, *reinterpret_cast<chash *>(hash));
  }

  static void cn_v7(const void *data, size_t length, char *hash) {
    cn_slow_hash_v7(*context, data, length, *reinterpret_cast<chash *>(hash));
  }

  static void cn_lite_v0(const void *data, size_t length, char *hash) {
    cn_lite_slow_hash_v0(*context, data, length, *reinterpret_cast<chash *>(hash));
  }

  static void cn_lite_v1(const void *data, size_t length, char *hash) {
    cn_lite_slow_hash_v1(*context, data, length, *reinterpret_cast<chash *>(hash));
  }
}

extern "C" typedef void hash_f(const void *, size_t, char *);
struct hash_func {
  const string name;
  hash_f &f;
} hashes[] = {{"fast", Crypto::cn_fast_hash},
              {"cryptonight-v6", cn_v6},
              {"cryptonight-v7", cn_v7},
              {"cryptonight-lite-v0", cn_lite_v0},
              {"cryptonight-lite-v1", cn_lite_v1},
              {"tree", hash_tree},
              {"extra-blake", Crypto::hash_extra_blake},
              {"extra-groestl", Crypto::hash_extra_groestl},
              {"extra-jh", Crypto::hash_extra_jh},
              {"extra-skein", Crypto::hash_extra_skein}};

int main(int argc, char *argv[]) {
  hash_f *f;
  hash_func *hf;
  fstream input;
  vector<char> data;
  chash expected, actual;
  size_t test = 0;
  bool error = false;
  if (argc != 3) {
    cerr << "Wrong number of arguments" << endl;
    return 1;
  }
  for (hf = hashes;; hf++) {
    if (hf >= &hashes[sizeof(hashes) / sizeof(hash_func)]) {
      cerr << "Unknown function" << endl;
      return 1;
    }
    if (argv[1] == hf->name) {
      f = &hf->f;
      break;
    }
  }
  if (f == cn_v6 || f == cn_v7 || f == cn_lite_v0 || f == cn_lite_v1 ) {
    context = new Crypto::cn_context();
  }
  input.open(argv[2], ios_base::in);
  for (;;) {
    ++test;
    input.exceptions(ios_base::badbit);
    get(input, expected);
    if (input.rdstate() & ios_base::eofbit) {
      break;
    }
    input.exceptions(ios_base::badbit | ios_base::failbit | ios_base::eofbit);
    input.clear(input.rdstate());
    get(input, data);
    f(data.data(), data.size(), (char *) &actual);
    if (expected != actual) {
      size_t i;
      cerr << "Hash mismatch on test " << test << endl << "Input: ";
      if (data.size() == 0) {
        cerr << "empty";
      } else {
        for (i = 0; i < data.size(); i++) {
          cerr << setbase(16) << setw(2) << setfill('0') << int(static_cast<unsigned char>(data[i]));
        }
      }
      cerr << endl << "Expected hash: ";
      for (i = 0; i < 32; i++) {
          cerr << setbase(16) << setw(2) << setfill('0') << int(reinterpret_cast<unsigned char *>(&expected)[i]);
      }
      cerr << endl << "Actual hash: ";
      for (i = 0; i < 32; i++) {
          cerr << setbase(16) << setw(2) << setfill('0') << int(reinterpret_cast<unsigned char *>(&actual)[i]);
      }
      cerr << endl;
      error = true;
    }
  }
  return error ? 1 : 0;
}
