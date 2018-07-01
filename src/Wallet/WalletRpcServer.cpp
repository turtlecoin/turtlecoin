// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "WalletRpcServer.h"

#include <fstream>

#include "Common/CommandLine.h"
#include "Common/StringTools.h"
#include "CryptoNoteCore/CryptoNoteFormatUtils.h"
#include "CryptoNoteCore/Account.h"
#include "crypto/hash.h"
#include "WalletLegacy/WalletHelper.h"
// #include "wallet_errors.h"

#include "Rpc/JsonRpc.h"

using namespace Logging;
using namespace CryptoNote;

namespace Tools
{

    const command_line::arg_descriptor<uint16_t> wallet_rpc_server::arg_rpc_bind_port = {"rpc-bind-port",
                                                                                         "Starts wallet as rpc server for wallet operations, sets bind port for server",
                                                                                         0, true};
    const command_line::arg_descriptor<std::string> wallet_rpc_server::arg_rpc_bind_ip = {"rpc-bind-ip",
                                                                                          "Specify ip to bind rpc server",
                                                                                          "127.0.0.1"};
    const command_line::arg_descriptor<std::string> wallet_rpc_server::arg_rpc_password = {"rpc-password",
                                                                                           "Specify the password to access the rpc server.",
                                                                                           "", true};
    const command_line::arg_descriptor<bool> wallet_rpc_server::arg_rpc_legacy_security = {"rpc-legacy-security",
                                                                                           "Enable legacy mode (no password for RPC). WARNING: INSECURE. USE ONLY AS A LAST RESORT.",
                                                                                           false};
    const command_line::arg_descriptor<bool> arg_allow_extended_rpc = {"allow-extended-rpc",
                                                                       "Allow RPC access to the wallet address and view/spend keys",
                                                                       false};


    void wallet_rpc_server::init_options(boost::program_options::options_description &desc)
    {
        command_line::add_arg(desc, arg_rpc_bind_ip);
        command_line::add_arg(desc, arg_rpc_bind_port);
        command_line::add_arg(desc, arg_rpc_password);
        command_line::add_arg(desc, arg_rpc_legacy_security);
        command_line::add_arg(desc, arg_allow_extended_rpc);
    }

//------------------------------------------------------------------------------------------------------------------------------
    wallet_rpc_server::wallet_rpc_server(
            System::Dispatcher &dispatcher,
            Logging::ILogger &log,
            CryptoNote::IWalletLegacy &w,
            CryptoNote::INode &n,
            CryptoNote::Currency &currency,
            const std::string &walletFile)
            :
            HttpServer(dispatcher, log),
            logger(log, "WalletRpc"),
            m_dispatcher(dispatcher),
            m_stopComplete(dispatcher),
            m_wallet(w),
            m_node(n),
            m_currency(currency),
            m_walletFilename(walletFile)
    {
    }

//------------------------------------------------------------------------------------------------------------------------------
    bool wallet_rpc_server::run()
    {
        start(m_bind_ip, m_port);
        m_stopComplete.wait();
        return true;
    }

    void wallet_rpc_server::send_stop_signal()
    {
        m_dispatcher.remoteSpawn([this]
                                 {
                                     std::cout << "wallet_rpc_server::send_stop_signal()" << std::endl;
                                     stop();
                                     m_stopComplete.set();
                                 });
    }

//------------------------------------------------------------------------------------------------------------------------------
    bool wallet_rpc_server::handle_command_line(const boost::program_options::variables_map &vm)
    {
        m_bind_ip = command_line::get_arg(vm, arg_rpc_bind_ip);
        m_port = command_line::get_arg(vm, arg_rpc_bind_port);
        m_legacy = command_line::get_arg(vm, arg_rpc_legacy_security);
        if (!m_legacy)
        {
            m_password = command_line::get_arg(vm, arg_rpc_password);
        }
        m_allow_extended_rpc = command_line::get_arg(vm, arg_allow_extended_rpc);
        return true;
    }

//------------------------------------------------------------------------------------------------------------------------------
    bool wallet_rpc_server::init(const boost::program_options::variables_map &vm)
    {
        if (!handle_command_line(vm))
        {
            logger(ERROR) << "Failed to process command line in wallet_rpc_server";
            return false;
        }

        return true;
    }

    void wallet_rpc_server::processRequest(const CryptoNote::HttpRequest &request, CryptoNote::HttpResponse &response)
    {

        using namespace CryptoNote::JsonRpc;

        JsonRpcRequest jsonRequest;
        JsonRpcResponse jsonResponse;
        std::string clientPassword;

        try
        {
            jsonRequest.parseRequest(request.getBody());
            jsonResponse.setId(jsonRequest.getId());

            if (!m_legacy)
            {
                const JsonRpc::OptionalPassword &clientPasswordObject = jsonRequest.getPassword();
                if (!clientPasswordObject.is_initialized())
                {
                    throw JsonRpcError(errInvalidPassword);
                }
                if (!clientPasswordObject.get().isString())
                {
                    throw JsonRpcError(errInvalidPassword);
                }
                clientPassword = clientPasswordObject.get().getString();
                if (clientPassword != m_password)
                {
                    throw JsonRpcError(errInvalidPassword);
                }
            }

            static std::unordered_map<std::string, JsonMemberMethod> s_methods = {
                    {"getbalance",    makeMemberMethod(&wallet_rpc_server::on_getbalance)},
                    {"store",         makeMemberMethod(&wallet_rpc_server::on_store)},
                    {"get_payments",  makeMemberMethod(&wallet_rpc_server::on_get_payments)},
                    {"get_transfers", makeMemberMethod(&wallet_rpc_server::on_get_transfers)},
                    {"get_height",    makeMemberMethod(&wallet_rpc_server::on_get_height)},
                    // below are the restricted methods, use --enable-extended-rpc
                    {"transfer",      makeMemberMethod(&wallet_rpc_server::on_transfer)},
                    {"reset",         makeMemberMethod(&wallet_rpc_server::on_reset)},
                    {"stop_wallet",   makeMemberMethod(&wallet_rpc_server::on_stop_wallet)},
                    {"get_address",   makeMemberMethod(&wallet_rpc_server::on_get_address)},
                    {"view_keys",     makeMemberMethod(&wallet_rpc_server::on_view_keys)}
            };

            auto it = s_methods.find(jsonRequest.getMethod());
            if (it == s_methods.end())
            {
                throw JsonRpcError(errMethodNotFound);
            }

            it->second(this, jsonRequest, jsonResponse);

        } catch (const JsonRpcError &err)
        {
            jsonResponse.setError(err);
        } catch (const std::exception &e)
        {
            jsonResponse.setError(JsonRpcError(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR, e.what()));
        }

        response.setBody(jsonResponse.getBody());
    }

//------------------------------------------------------------------------------------------------------------------------------
    bool wallet_rpc_server::on_getbalance(const wallet_rpc::COMMAND_RPC_GET_BALANCE::request &req,
                                          wallet_rpc::COMMAND_RPC_GET_BALANCE::response &res)
    {
        res.locked_amount = m_wallet.pendingBalance();
        res.available_balance = m_wallet.actualBalance();
        return true;
    }

//------------------------------------------------------------------------------------------------------------------------------
    bool wallet_rpc_server::on_transfer(const wallet_rpc::COMMAND_RPC_TRANSFER::request &req,
                                        wallet_rpc::COMMAND_RPC_TRANSFER::response &res)
    {

        if (!m_allow_extended_rpc)
        {
            throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_METHOD_RESTRICTED,
                                        "Unable to invoke extended RPC method without explicit --allow-extended-rpc flag.");
        }

        std::vector<CryptoNote::WalletLegacyTransfer> transfers;
        for (auto it = req.destinations.begin(); it != req.destinations.end(); it++)
        {
            CryptoNote::WalletLegacyTransfer transfer;
            transfer.address = it->address;
            transfer.amount = it->amount;
            transfers.push_back(transfer);
        }

        std::vector<uint8_t> extra;
        if (!req.payment_id.empty())
        {
            std::string payment_id_str = req.payment_id;

            Crypto::Hash payment_id;
            if (!CryptoNote::parsePaymentId(payment_id_str, payment_id))
            {
                throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID,
                                            "Payment id has invalid format: \"" + payment_id_str +
                                            "\", expected 64-character string");
            }

            BinaryArray extra_nonce;
            CryptoNote::setPaymentIdToTransactionExtraNonce(extra_nonce, payment_id);
            if (!CryptoNote::addExtraNonceToTransactionExtra(extra, extra_nonce))
            {
                throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID,
                                            "Something went wrong with payment_id. Please check its format: \"" +
                                            payment_id_str + "\", expected 64-character string");
            }
        }

        std::string extraString;
        std::copy(extra.begin(), extra.end(), std::back_inserter(extraString));
        try
        {
            CryptoNote::WalletHelper::SendCompleteResultObserver sent;
            WalletHelper::IWalletRemoveObserverGuard removeGuard(m_wallet, sent);

            CryptoNote::TransactionId tx = m_wallet.sendTransaction(transfers, req.fee, extraString, req.mixin,
                                                                    req.unlock_time);
            if (tx == WALLET_LEGACY_INVALID_TRANSACTION_ID)
            {
                throw std::runtime_error("Couldn't send transaction");
            }

            std::error_code sendError = sent.wait(tx);
            removeGuard.removeObserver();

            if (sendError)
            {
                throw std::system_error(sendError);
            }

            CryptoNote::WalletLegacyTransaction txInfo;
            m_wallet.getTransaction(tx, txInfo);
            res.tx_hash = Common::podToHex(txInfo.hash);

        } catch (const std::exception &e)
        {
            throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_GENERIC_TRANSFER_ERROR, e.what());
        }
        return true;
    }

//------------------------------------------------------------------------------------------------------------------------------
    bool wallet_rpc_server::on_store(const wallet_rpc::COMMAND_RPC_STORE::request &req,
                                     wallet_rpc::COMMAND_RPC_STORE::response &res)
    {
        try
        {
            WalletHelper::storeWallet(m_wallet, m_walletFilename);
        } catch (std::exception &e)
        {
            throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR,
                                        std::string("Couldn't save wallet: ") + e.what());
        }

        return true;
    }

//------------------------------------------------------------------------------------------------------------------------------
    bool wallet_rpc_server::on_get_payments(const wallet_rpc::COMMAND_RPC_GET_PAYMENTS::request &req,
                                            wallet_rpc::COMMAND_RPC_GET_PAYMENTS::response &res)
    {
        Crypto::Hash expectedPaymentId;
        CryptoNote::BinaryArray payment_id_blob;

        if (!Common::fromHex(req.payment_id, payment_id_blob))
        {
            throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID, "Payment ID has invald format");
        }

        if (sizeof(expectedPaymentId) != payment_id_blob.size())
        {
            throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_WRONG_PAYMENT_ID, "Payment ID has invalid size");
        }

        expectedPaymentId = *reinterpret_cast<const Crypto::Hash *>(payment_id_blob.data());
        size_t transactionsCount = m_wallet.getTransactionCount();
        for (size_t trantransactionNumber = 0; trantransactionNumber < transactionsCount; ++trantransactionNumber)
        {
            WalletLegacyTransaction txInfo;
            m_wallet.getTransaction(trantransactionNumber, txInfo);
            if (txInfo.state != WalletLegacyTransactionState::Active ||
                txInfo.blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT)
            {
                continue;
            }

            if (txInfo.totalAmount < 0) continue;
            std::vector<uint8_t> extraVec;
            extraVec.reserve(txInfo.extra.size());
            std::for_each(txInfo.extra.begin(), txInfo.extra.end(), [&extraVec](const char el)
            { extraVec.push_back(el); });

            Crypto::Hash paymentId;
            if (getPaymentIdFromTxExtra(extraVec, paymentId) && paymentId == expectedPaymentId)
            {
                wallet_rpc::payment_details rpc_payment;
                rpc_payment.tx_hash = Common::podToHex(txInfo.hash);
                rpc_payment.amount = txInfo.totalAmount;
                rpc_payment.block_height = txInfo.blockHeight;
                rpc_payment.unlock_time = txInfo.unlockTime;
                res.payments.push_back(rpc_payment);
            }
        }

        return true;
    }

    bool wallet_rpc_server::on_get_transfers(const wallet_rpc::COMMAND_RPC_GET_TRANSFERS::request &req,
                                             wallet_rpc::COMMAND_RPC_GET_TRANSFERS::response &res)
    {
        res.transfers.clear();
        size_t transactionsCount = m_wallet.getTransactionCount();
        for (size_t trantransactionNumber = 0; trantransactionNumber < transactionsCount; ++trantransactionNumber)
        {
            WalletLegacyTransaction txInfo;
            m_wallet.getTransaction(trantransactionNumber, txInfo);
            if (txInfo.state != WalletLegacyTransactionState::Active ||
                txInfo.blockHeight == WALLET_LEGACY_UNCONFIRMED_TRANSACTION_HEIGHT)
            {
                continue;
            }

            std::string address = "";
            if (txInfo.totalAmount < 0)
            {
                if (txInfo.transferCount > 0)
                {
                    WalletLegacyTransfer tr;
                    m_wallet.getTransfer(txInfo.firstTransferId, tr);
                    address = tr.address;
                }
            }

            wallet_rpc::Transfer transfer;
            transfer.time = txInfo.timestamp;
            transfer.output = txInfo.totalAmount < 0;
            transfer.transactionHash = Common::podToHex(txInfo.hash);
            transfer.amount = std::abs(txInfo.totalAmount);
            transfer.fee = txInfo.fee;
            transfer.address = address;
            transfer.blockIndex = txInfo.blockHeight;
            transfer.unlockTime = txInfo.unlockTime;
            transfer.paymentId = "";

            std::vector<uint8_t> extraVec;
            extraVec.reserve(txInfo.extra.size());
            std::for_each(txInfo.extra.begin(), txInfo.extra.end(), [&extraVec](const char el)
            { extraVec.push_back(el); });

            Crypto::Hash paymentId;
            transfer.paymentId = (getPaymentIdFromTxExtra(extraVec, paymentId) && paymentId != NULL_HASH
                                  ? Common::podToHex(paymentId) : "");

            res.transfers.push_back(transfer);
        }

        return true;
    }

    bool wallet_rpc_server::on_get_height(const wallet_rpc::COMMAND_RPC_GET_HEIGHT::request &req,
                                          wallet_rpc::COMMAND_RPC_GET_HEIGHT::response &res)
    {
        res.height = m_node.getLastLocalBlockHeight();
        return true;
    }

    bool wallet_rpc_server::on_reset(const wallet_rpc::COMMAND_RPC_RESET::request &req,
                                     wallet_rpc::COMMAND_RPC_RESET::response &res)
    {
        m_wallet.reset();
        return true;
    }

//------------------------------------------------------------------------------------------------------------------------------
    bool wallet_rpc_server::on_stop_wallet(const wallet_rpc::COMMAND_RPC_STOP::request &req,
                                           wallet_rpc::COMMAND_RPC_STOP::response &res)
    {

        if (m_allow_extended_rpc)
        {
            try
            {
                WalletHelper::storeWallet(m_wallet, m_walletFilename);
            } catch (std::exception &e)
            {
                throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_CODE_UNKNOWN_ERROR,
                                            std::string("Couldn't save wallet: ") + e.what());
            }

            wallet_rpc_server::send_stop_signal();
        } else
        {
            throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_METHOD_RESTRICTED,
                                        "Unable to invoke extended RPC method without explicit --allow-extended-rpc flag.");
        }
        return true;

    }

    bool wallet_rpc_server::on_get_address(const wallet_rpc::COMMAND_RPC_GET_ADDRESS::request &req,
                                           wallet_rpc::COMMAND_RPC_GET_ADDRESS::response &res)
    {

        if (m_allow_extended_rpc)
        {
            res.address = m_wallet.getAddress();
        } else
        {
            throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_METHOD_RESTRICTED,
                                        "Unable to invoke extended RPC method without explicit --allow-extended-rpc flag.");
        }
        return true;

    }

    bool wallet_rpc_server::on_view_keys(const wallet_rpc::COMMAND_RPC_VIEW_KEYS::request &req,
                                         wallet_rpc::COMMAND_RPC_VIEW_KEYS::response &res)
    {

        if (m_allow_extended_rpc)
        {
            AccountKeys keys;
            m_wallet.getAccountKeys(keys);
            res.view_key = Common::podToHex(keys.viewSecretKey);
            res.spend_key = Common::podToHex(keys.spendSecretKey);
        } else
        {
            throw JsonRpc::JsonRpcError(WALLET_RPC_ERROR_METHOD_RESTRICTED,
                                        "Unable to invoke extended RPC method without explicit --allow-extended-rpc flag.");
        }
        return true;

    }

}
