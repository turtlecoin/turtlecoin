// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2016-2018, The Karbowanec developers
// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

#pragma once

#include <cstddef>
#include <limits>
#include <mutex>
#include <type_traits>
#include <vector>

#include <CryptoTypes.h>

#include "hash.h"

namespace Crypto {

struct EllipticCurvePoint {
  uint8_t data[32];
};

struct EllipticCurveScalar {
  uint8_t data[32];
};

  class crypto_ops {
    crypto_ops();
    crypto_ops(const crypto_ops &);
    void operator=(const crypto_ops &);
    ~crypto_ops();

    static void generate_keys(PublicKey &, SecretKey &);
    friend void generate_keys(PublicKey &, SecretKey &);
	static void generate_deterministic_keys(PublicKey &pub, SecretKey &sec, SecretKey& second);
	friend void generate_deterministic_keys(PublicKey &pub, SecretKey &sec, SecretKey& second);
	static SecretKey generate_m_keys(PublicKey &pub, SecretKey &sec, const SecretKey& recovery_key = SecretKey(), bool recover = false);
	friend SecretKey generate_m_keys(PublicKey &pub, SecretKey &sec, const SecretKey& recovery_key, bool recover);
    static bool check_key(const PublicKey &);
    friend bool check_key(const PublicKey &);
    static bool secret_key_to_public_key(const SecretKey &, PublicKey &);
    friend bool secret_key_to_public_key(const SecretKey &, PublicKey &);
    static bool generate_key_derivation(const PublicKey &, const SecretKey &, KeyDerivation &);
    friend bool generate_key_derivation(const PublicKey &, const SecretKey &, KeyDerivation &);
    static bool derive_public_key(const KeyDerivation &, size_t, const PublicKey &, PublicKey &);
    friend bool derive_public_key(const KeyDerivation &, size_t, const PublicKey &, PublicKey &);
    friend bool derive_public_key(const KeyDerivation &, size_t, const PublicKey &, const uint8_t*, size_t, PublicKey &);
    static bool derive_public_key(const KeyDerivation &, size_t, const PublicKey &, const uint8_t*, size_t, PublicKey &);
    //hack for pg
    static bool underive_public_key_and_get_scalar(const KeyDerivation &, std::size_t, const PublicKey &, PublicKey &, EllipticCurveScalar &);
    friend bool underive_public_key_and_get_scalar(const KeyDerivation &, std::size_t, const PublicKey &, PublicKey &, EllipticCurveScalar &);
    //
    static void derive_secret_key(const KeyDerivation &, size_t, const SecretKey &, SecretKey &);
    friend void derive_secret_key(const KeyDerivation &, size_t, const SecretKey &, SecretKey &);
    static void derive_secret_key(const KeyDerivation &, size_t, const SecretKey &, const uint8_t*, size_t, SecretKey &);
    friend void derive_secret_key(const KeyDerivation &, size_t, const SecretKey &, const uint8_t*, size_t, SecretKey &);
    static bool underive_public_key(const KeyDerivation &, size_t, const PublicKey &, PublicKey &);
    friend bool underive_public_key(const KeyDerivation &, size_t, const PublicKey &, PublicKey &);
    static bool underive_public_key(const KeyDerivation &, size_t, const PublicKey &, const uint8_t*, size_t, PublicKey &);
    friend bool underive_public_key(const KeyDerivation &, size_t, const PublicKey &, const uint8_t*, size_t, PublicKey &);
    static void generate_signature(const Hash &, const PublicKey &, const SecretKey &, Signature &);
    friend void generate_signature(const Hash &, const PublicKey &, const SecretKey &, Signature &);
    static bool check_signature(const Hash &, const PublicKey &, const Signature &);
    friend bool check_signature(const Hash &, const PublicKey &, const Signature &);
    static void generate_key_image(const PublicKey &, const SecretKey &, KeyImage &);
    friend void generate_key_image(const PublicKey &, const SecretKey &, KeyImage &);
    static KeyImage scalarmultKey(const KeyImage & P, const KeyImage & a);
    friend KeyImage scalarmultKey(const KeyImage & P, const KeyImage & a);
    static void hash_data_to_ec(const uint8_t*, std::size_t, PublicKey&);
    friend void hash_data_to_ec(const uint8_t*, std::size_t, PublicKey&);

    public:

        static std::tuple<bool, std::vector<Signature>> generateRingSignatures(
            const Hash prefixHash,
            const KeyImage keyImage,
            const std::vector<PublicKey> publicKeys,
            const Crypto::SecretKey transactionSecretKey,
            uint64_t realOutput);

        static bool checkRingSignature(
            const Hash &prefix_hash,
            const KeyImage &image,
            const std::vector<PublicKey> pubs,
            const std::vector<Signature> signatures);

        static void generateViewFromSpend(
            const Crypto::SecretKey &spend,
            Crypto::SecretKey &viewSecret);

        static void generateViewFromSpend(
            const Crypto::SecretKey &spend,
            Crypto::SecretKey &viewSecret,
            Crypto::PublicKey &viewPublic);
  };

  /* Generate a new key pair
   */
  inline void generate_keys(PublicKey &pub, SecretKey &sec) {
    crypto_ops::generate_keys(pub, sec);
  }

  inline void generate_deterministic_keys(PublicKey &pub, SecretKey &sec, SecretKey& second) {
    crypto_ops::generate_deterministic_keys(pub, sec, second);
  }

  inline SecretKey generate_m_keys(PublicKey &pub, SecretKey &sec, const SecretKey& recovery_key = SecretKey(), bool recover = false) {
    return crypto_ops::generate_m_keys(pub, sec, recovery_key, recover);
  }

  /* Check a public key. Returns true if it is valid, false otherwise.
   */
  inline bool check_key(const PublicKey &key) {
    return crypto_ops::check_key(key);
  }

  /* Checks a private key and computes the corresponding public key.
   */
  inline bool secret_key_to_public_key(const SecretKey &sec, PublicKey &pub) {
    return crypto_ops::secret_key_to_public_key(sec, pub);
  }

  /* To generate an ephemeral key used to send money to:
   * * The sender generates a new key pair, which becomes the transaction key. The public transaction key is included in "extra" field.
   * * Both the sender and the receiver generate key derivation from the transaction key and the receivers' "view" key.
   * * The sender uses key derivation, the output index, and the receivers' "spend" key to derive an ephemeral public key.
   * * The receiver can either derive the public key (to check that the transaction is addressed to him) or the private key (to spend the money).
   */
  inline bool generate_key_derivation(const PublicKey &key1, const SecretKey &key2, KeyDerivation &derivation) {
    return crypto_ops::generate_key_derivation(key1, key2, derivation);
  }

  inline bool derive_public_key(const KeyDerivation &derivation, size_t output_index,
    const PublicKey &base, const uint8_t* prefix, size_t prefixLength, PublicKey &derived_key) {
    return crypto_ops::derive_public_key(derivation, output_index, base, prefix, prefixLength, derived_key);
  }

  inline bool derive_public_key(const KeyDerivation &derivation, size_t output_index,
    const PublicKey &base, PublicKey &derived_key) {
    return crypto_ops::derive_public_key(derivation, output_index, base, derived_key);
  }


  inline bool underive_public_key_and_get_scalar(const KeyDerivation &derivation, std::size_t output_index,
    const PublicKey &derived_key, PublicKey &base, EllipticCurveScalar &hashed_derivation) {
    return crypto_ops::underive_public_key_and_get_scalar(derivation, output_index, derived_key, base, hashed_derivation);
  }
  
  inline void derive_secret_key(const KeyDerivation &derivation, std::size_t output_index,
    const SecretKey &base, const uint8_t* prefix, size_t prefixLength, SecretKey &derived_key) {
    crypto_ops::derive_secret_key(derivation, output_index, base, prefix, prefixLength, derived_key);
  }

  inline void derive_secret_key(const KeyDerivation &derivation, std::size_t output_index,
    const SecretKey &base, SecretKey &derived_key) {
    crypto_ops::derive_secret_key(derivation, output_index, base, derived_key);
  }


  /* Inverse function of derive_public_key. It can be used by the receiver to find which "spend" key was used to generate a transaction. This may be useful if the receiver used multiple addresses which only differ in "spend" key.
   */
  inline bool underive_public_key(const KeyDerivation &derivation, size_t output_index,
    const PublicKey &derived_key, const uint8_t* prefix, size_t prefixLength, PublicKey &base) {
    return crypto_ops::underive_public_key(derivation, output_index, derived_key, prefix, prefixLength, base);
  }

  inline bool underive_public_key(const KeyDerivation &derivation, size_t output_index,
    const PublicKey &derived_key, PublicKey &base) {
    return crypto_ops::underive_public_key(derivation, output_index, derived_key, base);
  }

  /* Generation and checking of a standard signature.
   */
  inline void generate_signature(const Hash &prefix_hash, const PublicKey &pub, const SecretKey &sec, Signature &sig) {
    crypto_ops::generate_signature(prefix_hash, pub, sec, sig);
  }
  inline bool check_signature(const Hash &prefix_hash, const PublicKey &pub, const Signature &sig) {
    return crypto_ops::check_signature(prefix_hash, pub, sig);
  }

  /* To send money to a key:
   * * The sender generates an ephemeral key and includes it in transaction output.
   * * To spend the money, the receiver generates a key image from it.
   * * Then he selects a bunch of outputs, including the one he spends, and uses them to generate a ring signature.
   * To check the signature, it is necessary to collect all the keys that were used to generate it. To detect double spends, it is necessary to check that each key image is used at most once.
   */
  inline void generate_key_image(const PublicKey &pub, const SecretKey &sec, KeyImage &image) {
    crypto_ops::generate_key_image(pub, sec, image);
  }

  inline KeyImage scalarmultKey(const KeyImage & P, const KeyImage & a) {
    return crypto_ops::scalarmultKey(P, a);
  }

  inline void hash_data_to_ec(const uint8_t* data, std::size_t len, PublicKey& key) {
    crypto_ops::hash_data_to_ec(data, len, key);
  }
}
