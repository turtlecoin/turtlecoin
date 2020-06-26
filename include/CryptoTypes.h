// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2020, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include <algorithm>
#include <common/StringTools.h>
#include <JsonHelper.h>
#include <cstdint>
#include <iterator>

namespace Crypto
{
    struct EllipticCurvePoint
    {
        EllipticCurvePoint() {}

        EllipticCurvePoint(std::initializer_list<uint8_t> input)
        {
            std::copy(input.begin(), input.end(), std::begin(data));
        }

        EllipticCurvePoint(const uint8_t input[32])
        {
            std::copy(input, input + 32, std::begin(data));
        }

        EllipticCurvePoint(const std::string &s)
        {
            fromString(s);
        }

        bool operator==(const EllipticCurvePoint &other) const
        {
            return std::equal(std::begin(data), std::end(data), std::begin(other.data));
        }

        bool operator!=(const EllipticCurvePoint &other) const
        {
            return !(*this == other);
        }

        /* Converts the class to a json object */
        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.String(Common::podToHex(data));
        }

        void fromJSON(const JSONValue &j)
        {
            if (j.IsString())
            {
                fromString(j.GetString());
            }
        }

        void fromJSON(const JSONValue &j, const std::string &key)
        {
            auto val = j.FindMember(key);

            if (val == j.MemberEnd())
            {
                throw std::invalid_argument("Missing JSON parameter: '" + key + "'");
            }

            fromJSON(val->value);
        }

        /* Initializes the class from a json string */
        void fromString(const std::string &s)
        {
            if (!Common::podFromHex(s, data))
            {
                throw std::invalid_argument("Error parsing JSON EllipticCurvePoint, wrong length or not hex");
            }
        }

        uint8_t data[32];
    };

    struct EllipticCurveScalar
    {
        EllipticCurveScalar() {}

        EllipticCurveScalar(std::initializer_list<uint8_t> input)
        {
            std::copy(input.begin(), input.end(), std::begin(data));
        }

        EllipticCurveScalar(const uint8_t input[32])
        {
            std::copy(input, input + 32, std::begin(data));
        }

        EllipticCurveScalar(const std::string &s)
        {
            fromString(s);
        }

        bool operator==(const EllipticCurveScalar &other) const
        {
            return std::equal(std::begin(data), std::end(data), std::begin(other.data));
        }

        bool operator!=(const EllipticCurveScalar &other) const
        {
            return !(*this == other);
        }

        /* Converts the class to a json object */
        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.String(Common::podToHex(data));
        }

        void fromJSON(const JSONValue &j)
        {
            if (j.IsString())
            {
                fromString(j.GetString());
            }
        }

        void fromJSON(const JSONValue &j, const std::string &key)
        {
            auto val = j.FindMember(key);

            if (val == j.MemberEnd())
            {
                throw std::invalid_argument("Missing JSON parameter: '" + key + "'");
            }

            fromJSON(val->value);
        }

        /* Initializes the class from a json string */
        void fromString(const std::string &s)
        {
            if (!Common::podFromHex(s, data))
            {
                throw std::invalid_argument("Error parsing JSON EllipticCurveScalar, wrong length or not hex");
            }
        }

        uint8_t data[32];
    };

    struct Hash
    {
        /* Can't have constructors here, because it violates std::is_pod<>
           which is used somewhere */
        bool operator==(const Hash &other) const
        {
            return std::equal(std::begin(data), std::end(data), std::begin(other.data));
        }

        bool operator!=(const Hash &other) const
        {
            return !(*this == other);
        }

        uint8_t data[32];

        /* Converts the class to a json object */
        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.String(Common::podToHex(data));
        }

        void fromJSON(const JSONValue &j)
        {
            if (j.IsString())
            {
                fromString(j.GetString());
            }
        }

        void fromJSON(const JSONValue &j, const std::string &key)
        {
            auto val = j.FindMember(key);

            if (val == j.MemberEnd())
            {
                throw std::invalid_argument("Missing JSON parameter: '" + key + "'");
            }

            fromJSON(val->value);
        }

        /* Initializes the class from a json string */
        void fromString(const std::string &s)
        {
            if (!Common::podFromHex(s, data))
            {
                throw std::invalid_argument("Error parsing JSON Hash, wrong length or not hex");
            }
        }
    };

    struct PublicKey
    {
        PublicKey() {}

        PublicKey(std::initializer_list<uint8_t> input)
        {
            std::copy(input.begin(), input.end(), std::begin(data));
        }

        PublicKey(const uint8_t input[32])
        {
            std::copy(input, input + 32, std::begin(data));
        }

        PublicKey(const std::string &s)
        {
            fromString(s);
        }

        bool operator==(const PublicKey &other) const
        {
            return std::equal(std::begin(data), std::end(data), std::begin(other.data));
        }

        bool operator!=(const PublicKey &other) const
        {
            return !(*this == other);
        }

        /* Converts the class to a json object */
        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.String(Common::podToHex(data));
        }

        void fromJSON(const JSONValue &j)
        {
            if (j.IsString())
            {
                fromString(j.GetString());
            }
        }

        void fromJSON(const JSONValue &j, const std::string &key)
        {
            auto val = j.FindMember(key);

            if (val == j.MemberEnd())
            {
                throw std::invalid_argument("Missing JSON parameter: '" + key + "'");
            }

            fromJSON(val->value);
        }

        /* Initializes the class from a json string */
        void fromString(const std::string &s)
        {
            if (!Common::podFromHex(s, data))
            {
                throw std::invalid_argument("Error parsing JSON PublicKey, wrong length or not hex");
            }
        }

        uint8_t data[32];
    };

    struct SecretKey
    {
        SecretKey() {}

        SecretKey(std::initializer_list<uint8_t> input)
        {
            std::copy(input.begin(), input.end(), std::begin(data));
        }

        SecretKey(const uint8_t input[32])
        {
            std::copy(input, input + 32, std::begin(data));
        }

        SecretKey(const std::string &s)
        {
            fromString(s);
        }

        bool operator==(const SecretKey &other) const
        {
            return std::equal(std::begin(data), std::end(data), std::begin(other.data));
        }

        bool operator!=(const SecretKey &other) const
        {
            return !(*this == other);
        }

        /* Converts the class to a json object */
        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.String(Common::podToHex(data));
        }

        void fromJSON(const JSONValue &j)
        {
            if (j.IsString())
            {
                fromString(j.GetString());
            }
        }

        void fromJSON(const JSONValue &j, const std::string &key)
        {
            auto val = j.FindMember(key);

            if (val == j.MemberEnd())
            {
                throw std::invalid_argument("Missing JSON parameter: '" + key + "'");
            }

            fromJSON(val->value);
        }

        /* Initializes the class from a json string */
        void fromString(const std::string &s)
        {
            if (!Common::podFromHex(s, data))
            {
                throw std::invalid_argument("Error parsing JSON SecretKey, wrong length or not hex");
            }
        }

        uint8_t data[32];
    };

    struct KeyDerivation
    {
        KeyDerivation() {}

        KeyDerivation(std::initializer_list<uint8_t> input)
        {
            std::copy(input.begin(), input.end(), std::begin(data));
        }

        KeyDerivation(const uint8_t input[32])
        {
            std::copy(input, input + 32, std::begin(data));
        }

        KeyDerivation(const std::string &s)
        {
            fromString(s);
        }

        bool operator==(const KeyDerivation &other) const
        {
            return std::equal(std::begin(data), std::end(data), std::begin(other.data));
        }

        bool operator!=(const KeyDerivation &other) const
        {
            return !(*this == other);
        }

        /* Converts the class to a json object */
        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.String(Common::podToHex(data));
        }

        void fromJSON(const JSONValue &j)
        {
            if (j.IsString())
            {
                fromString(j.GetString());
            }
        }

        void fromJSON(const JSONValue &j, const std::string &key)
        {
            auto val = j.FindMember(key);

            if (val == j.MemberEnd())
            {
                throw std::invalid_argument("Missing JSON parameter: '" + key + "'");
            }

            fromJSON(val->value);
        }

        /* Initializes the class from a json string */
        void fromString(const std::string &s)
        {
            if (!Common::podFromHex(s, data))
            {
                throw std::invalid_argument("Error parsing JSON KeyDerivation, wrong length or not hex");
            }
        }

        uint8_t data[32];
    };

    struct KeyImage
    {
        KeyImage() {}

        KeyImage(std::initializer_list<uint8_t> input)
        {
            std::copy(input.begin(), input.end(), std::begin(data));
        }

        KeyImage(const uint8_t input[32])
        {
            std::copy(input, input + 32, std::begin(data));
        }

        KeyImage(const std::string &s)
        {
            fromString(s);
        }

        bool operator==(const KeyImage &other) const
        {
            return std::equal(std::begin(data), std::end(data), std::begin(other.data));
        }

        bool operator!=(const KeyImage &other) const
        {
            return !(*this == other);
        }

        /* Converts the class to a json object */
        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.String(Common::podToHex(data));
        }

        void fromJSON(const JSONValue &j)
        {
            if (j.IsString())
            {
                fromString(j.GetString());
            }
        }

        void fromJSON(const JSONValue &j, const std::string &key)
        {
            auto val = j.FindMember(key);

            if (val == j.MemberEnd())
            {
                throw std::invalid_argument("Missing JSON parameter: '" + key + "'");
            }

            fromJSON(val->value);
        }

        /* Initializes the class from a json string */
        void fromString(const std::string &s)
        {
            if (!Common::podFromHex(s, data))
            {
                throw std::invalid_argument("Error parsing JSON keyimage, wrong length or not hex");
            }
        }

        uint8_t data[32];
    };

    struct Signature
    {
        Signature() {}

        Signature(std::initializer_list<uint8_t> input)
        {
            std::copy(input.begin(), input.end(), std::begin(data));
        }

        Signature(const uint8_t input[64])
        {
            std::copy(input, input + 64, std::begin(data));
        }

        Signature(const std::string &s)
        {
            fromString(s);
        }

        bool operator==(const Signature &other) const
        {
            return std::equal(std::begin(data), std::end(data), std::begin(other.data));
        }

        bool operator!=(const Signature &other) const
        {
            return !(*this == other);
        }

        /* Converts the class to a json object */
        void toJSON(rapidjson::Writer<rapidjson::StringBuffer> &writer) const
        {
            writer.String(Common::podToHex(data));
        }

        void fromJSON(const JSONValue &j)
        {
            if (j.IsString())
            {
                fromString(j.GetString());
            }
        }

        void fromJSON(const JSONValue &j, const std::string &key)
        {
            auto val = j.FindMember(key);

            if (val == j.MemberEnd())
            {
                throw std::invalid_argument("Missing JSON parameter: '" + key + "'");
            }

            fromJSON(val->value);
        }

        /* Initializes the class from a json string */
        void fromString(const std::string &s)
        {
            if (!Common::podFromHex(s, data))
            {
                throw std::invalid_argument("Error parsing JSON keyimage, wrong length or not hex");
            }
        }

        uint8_t data[64] = {};
    };

    /* For boost hash_value */
    inline size_t hash_value(const EllipticCurvePoint &ep)
    {
        return reinterpret_cast<const size_t &>(ep);
    }

    inline size_t hash_value(const EllipticCurveScalar &es)
    {
        return reinterpret_cast<const size_t &>(es);
    }

    inline size_t hash_value(const Hash &hash)
    {
        return reinterpret_cast<const size_t &>(hash);
    }

    inline size_t hash_value(const PublicKey &publicKey)
    {
        return reinterpret_cast<const size_t &>(publicKey);
    }

    inline size_t hash_value(const SecretKey &secretKey)
    {
        return reinterpret_cast<const size_t &>(secretKey);
    }

    inline size_t hash_value(const KeyDerivation &keyDerivation)
    {
        return reinterpret_cast<const size_t &>(keyDerivation);
    }

    inline size_t hash_value(const KeyImage &keyImage)
    {
        return reinterpret_cast<const size_t &>(keyImage);
    }
} // namespace Crypto

namespace std
{
    /* For using in std::unordered_* containers */
    template<> struct hash<Crypto::EllipticCurvePoint>
    {
        size_t operator()(const Crypto::EllipticCurvePoint &ep) const
        {
            return reinterpret_cast<const size_t &>(ep);
        }
    };

    template<> struct hash<Crypto::EllipticCurveScalar>
    {
        size_t operator()(const Crypto::EllipticCurveScalar &es) const
        {
            return reinterpret_cast<const size_t &>(es);
        }
    };

    template<> struct hash<Crypto::Hash>
    {
        size_t operator()(const Crypto::Hash &hash) const
        {
            return reinterpret_cast<const size_t &>(hash);
        }
    };

    template<> struct hash<Crypto::PublicKey>
    {
        size_t operator()(const Crypto::PublicKey &publicKey) const
        {
            return reinterpret_cast<const size_t &>(publicKey);
        }
    };

    template<> struct hash<Crypto::SecretKey>
    {
        size_t operator()(const Crypto::SecretKey &secretKey) const
        {
            return reinterpret_cast<const size_t &>(secretKey);
        }
    };

    template<> struct hash<Crypto::KeyDerivation>
    {
        size_t operator()(const Crypto::KeyDerivation &keyDerivation) const
        {
            return reinterpret_cast<const size_t &>(keyDerivation);
        }
    };

    template<> struct hash<Crypto::KeyImage>
    {
        size_t operator()(const Crypto::KeyImage &keyImage) const
        {
            return reinterpret_cast<const size_t &>(keyImage);
        }
    };

    template<> struct hash<Crypto::Signature>
    {
        size_t operator()(const Crypto::Signature &signature) const
        {
            return reinterpret_cast<const size_t &>(signature);
        }
    };

    /* Overloading the << operator */
    inline ostream &operator<<(ostream &os, const Crypto::EllipticCurvePoint &ep)
    {
        os << Common::podToHex(ep);
        return os;
    }

    inline ostream &operator<<(ostream &os, const Crypto::EllipticCurveScalar &es)
    {
        os << Common::podToHex(es);
        return os;
    }

    inline ostream &operator<<(ostream &os, const Crypto::Hash &hash)
    {
        os << Common::podToHex(hash);
        return os;
    }

    inline ostream &operator<<(ostream &os, const Crypto::PublicKey &publicKey)
    {
        os << Common::podToHex(publicKey);
        return os;
    }

    inline ostream &operator<<(ostream &os, const Crypto::SecretKey &secretKey)
    {
        os << Common::podToHex(secretKey);
        return os;
    }

    inline ostream &operator<<(ostream &os, const Crypto::KeyDerivation &keyDerivation)
    {
        os << Common::podToHex(keyDerivation);
        return os;
    }

    inline ostream &operator<<(ostream &os, const Crypto::KeyImage &keyImage)
    {
        os << Common::podToHex(keyImage);
        return os;
    }

    inline ostream &operator<<(ostream &os, const Crypto::Signature &signature)
    {
        os << Common::podToHex(signature);
        return os;
    }
} // namespace std
