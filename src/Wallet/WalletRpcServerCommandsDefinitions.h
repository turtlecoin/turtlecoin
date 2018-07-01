// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include "CryptoNoteProtocol/CryptoNoteProtocolDefinitions.h"
#include "CryptoNoteCore/CryptoNoteBasic.h"
#include "crypto/hash.h"
#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "WalletRpcServerErrorCodes.h"

namespace Tools
{
    namespace wallet_rpc
    {

        using CryptoNote::ISerializer;

#define WALLET_RPC_STATUS_OK      "OK"
#define WALLET_RPC_STATUS_BUSY    "BUSY"

        struct COMMAND_RPC_GET_BALANCE
        {
            typedef CryptoNote::EMPTY_STRUCT request;

            struct response
            {
                uint64_t locked_amount;
                uint64_t available_balance;

                void serialize(ISerializer &s)
                {
                    KV_MEMBER(locked_amount)
                    KV_MEMBER(available_balance)
                }
            };
        };

        struct transfer_destination
        {
            uint64_t amount;
            std::string address;

            void serialize(ISerializer &s)
            {
                KV_MEMBER(amount)
                KV_MEMBER(address)
            }
        };

        struct COMMAND_RPC_TRANSFER
        {
            struct request
            {
                std::list<transfer_destination> destinations;
                uint64_t fee;
                uint64_t mixin;
                uint64_t unlock_time;
                std::string payment_id;

                void serialize(ISerializer &s)
                {
                    KV_MEMBER(destinations)
                    KV_MEMBER(fee)
                    KV_MEMBER(mixin)
                    KV_MEMBER(unlock_time)
                    KV_MEMBER(payment_id)
                }
            };

            struct response
            {
                std::string tx_hash;

                void serialize(ISerializer &s)
                {
                    KV_MEMBER(tx_hash)
                }
            };
        };

        struct COMMAND_RPC_STORE
        {
            typedef CryptoNote::EMPTY_STRUCT request;
            typedef CryptoNote::EMPTY_STRUCT response;
        };

        struct COMMAND_RPC_STOP
        {
            typedef CryptoNote::EMPTY_STRUCT request;
            typedef CryptoNote::EMPTY_STRUCT response;
        };

        struct payment_details
        {
            std::string tx_hash;
            uint64_t amount;
            uint64_t block_height;
            uint64_t unlock_time;

            void serialize(ISerializer &s)
            {
                KV_MEMBER(tx_hash)
                KV_MEMBER(amount)
                KV_MEMBER(block_height)
                KV_MEMBER(unlock_time)
            }
        };

        struct COMMAND_RPC_GET_PAYMENTS
        {
            struct request
            {
                std::string payment_id;

                void serialize(ISerializer &s)
                {
                    KV_MEMBER(payment_id)
                }
            };

            struct response
            {
                std::list<payment_details> payments;

                void serialize(ISerializer &s)
                {
                    KV_MEMBER(payments)
                }
            };
        };

        struct Transfer
        {
            uint64_t time;
            bool output;
            std::string transactionHash;
            uint64_t amount;
            uint64_t fee;
            std::string paymentId;
            std::string address;
            uint64_t blockIndex;
            uint64_t unlockTime;

            void serialize(ISerializer &s)
            {
                KV_MEMBER(time)
                KV_MEMBER(output)
                KV_MEMBER(transactionHash)
                KV_MEMBER(amount)
                KV_MEMBER(fee)
                KV_MEMBER(paymentId)
                KV_MEMBER(address)
                KV_MEMBER(blockIndex)
                KV_MEMBER(unlockTime)
            }
        };

        struct COMMAND_RPC_GET_TRANSFERS
        {
            typedef CryptoNote::EMPTY_STRUCT request;

            struct response
            {
                std::list<Transfer> transfers;

                void serialize(ISerializer &s)
                {
                    KV_MEMBER(transfers)
                }
            };
        };

        struct COMMAND_RPC_GET_HEIGHT
        {
            typedef CryptoNote::EMPTY_STRUCT request;

            struct response
            {
                uint64_t height;

                void serialize(ISerializer &s)
                {
                    KV_MEMBER(height)
                }
            };
        };

        struct COMMAND_RPC_RESET
        {
            typedef CryptoNote::EMPTY_STRUCT request;
            typedef CryptoNote::EMPTY_STRUCT response;
        };

        struct COMMAND_RPC_RESET_FROM
        {

            struct request
            {
                uint64_t height;

                void serialize(ISerializer &s)
                {
                    KV_MEMBER(height)
                }
            };

            typedef CryptoNote::EMPTY_STRUCT response;

        };

        struct COMMAND_RPC_GET_ADDRESS
        {
            typedef CryptoNote::EMPTY_STRUCT request;

            struct response
            {
                std::string address;

                void serialize(ISerializer &s)
                {
                    KV_MEMBER(address)
                }
            };
        };

        struct COMMAND_RPC_VIEW_KEYS
        {
            typedef CryptoNote::EMPTY_STRUCT request;

            struct response
            {
                std::string view_key;
                std::string spend_key;

                void serialize(ISerializer &s)
                {
                    KV_MEMBER(view_key)
                    KV_MEMBER(spend_key)
                }

            };
        };

    }
}
