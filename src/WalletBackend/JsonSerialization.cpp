// Copyright (c) 2018, The TurtleCoin Developers
// 
// Please see the included LICENSE file for more information.

////////////////////////////////////////////
#include <WalletBackend/JsonSerialization.h>
////////////////////////////////////////////

#include <WalletBackend/WalletBackend.h>
#include <WalletBackend/SubWallet.h>

using nlohmann::json;

///////////////
/* SubWallet */
///////////////

void to_json(json &j, const SubWallet &s)
{
    j = s.toJson();
}

void from_json(const json &j, SubWallet &s)
{
    s.fromJson(j);
}

json SubWallet::toJson() const
{
    return
    {
        {"privateSpendKey", m_privateSpendKey},
        {"address", m_address},
        {"scanHeight", m_scanHeight},
        {"creationTimestamp", m_creationTimestamp},
    };
}

void SubWallet::fromJson(const json &j)
{
    m_privateSpendKey = j.at("privateSpendKey").get<Crypto::SecretKey>();
    m_address = j.at("address").get<std::string>();
    m_scanHeight = j.at("scanHeight").get<uint64_t>();
    m_creationTimestamp = j.at("creationTimestamp").get<uint64_t>();
}

///////////////////
/* WalletBackend */
///////////////////

void to_json(json &j, const WalletBackend &w)
{
    j = w.toJson();
}

void from_json(const json &j, WalletBackend &w)
{
    w.fromJson(j);
}

json WalletBackend::toJson() const
{
    return
    {
        {"walletFileFormatVersion", WALLET_FILE_FORMAT_VERSION},
        {"privateViewKey", m_privateViewKey},
        {"subWallets", m_subWallets},
        {"isViewWallet", m_isViewWallet},
    };
}

void WalletBackend::fromJson(const json &j)
{
    uint16_t version = j.at("walletFileFormatVersion").get<uint16_t>();

    if (version != WALLET_FILE_FORMAT_VERSION)
    {
        /* TODO: This should probably be a custom type, throwing an actual
           error we can catch upstream? */
        throw std::invalid_argument(
            "Wallet file format version is not supported by this version of "
            "the software!"
        );
    }

    m_privateViewKey = j.at("privateViewKey").get<Crypto::SecretKey>();
    m_subWallets = j.at("subWallets").get<std::unordered_map<std::string, SubWallet>>();
    m_isViewWallet = j.at("isViewWallet").get<bool>();
}

///////////////////////
/* Crypto::SecretKey */
///////////////////////

/* Declaration of to_json and from_json have to be in the same namespace as
   the type itself was declared in */
namespace Crypto
{

void to_json(json &j, const Crypto::SecretKey &s)
{
    j = json{
        {"secretKey", s.data}
    };
}

void from_json(const json &j, Crypto::SecretKey &s)
{
    auto data = j.at("secretKey").get<std::array<uint8_t, sizeof(Crypto::SecretKey::data)>>();

    for (size_t i = 0; i < data.size(); i++)
    {
        s.data[i] = data[i];
    }
}

}
