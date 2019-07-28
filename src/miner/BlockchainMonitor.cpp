// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "BlockchainMonitor.h"

#include "common/StringTools.h"
#include "rpc/CoreRpcServerCommandsDefinitions.h"
#include "rpc/JsonRpc.h"

#include <system/EventLock.h>
#include <system/InterruptedException.h>
#include <system/Timer.h>
#include <utilities/ColouredMsg.h>

using json = nlohmann::json;

BlockchainMonitor::BlockchainMonitor(
    System::Dispatcher &dispatcher,
    const size_t pollingInterval,
    const std::shared_ptr<httplib::Client> httpClient):

    m_dispatcher(dispatcher),
    m_pollingInterval(pollingInterval),
    m_stopped(false),
    m_sleepingContext(dispatcher),
    m_httpClient(httpClient)
{
}

void BlockchainMonitor::waitBlockchainUpdate()
{
    m_stopped = false;

    auto lastBlockHash = requestLastBlockHash();

    while (!lastBlockHash && !m_stopped)
    {
        std::this_thread::sleep_for(std::chrono::seconds(m_pollingInterval));
        lastBlockHash = requestLastBlockHash();
    }

    while (!m_stopped)
    {
        m_sleepingContext.spawn([this]() {
            System::Timer timer(m_dispatcher);
            timer.sleep(std::chrono::seconds(m_pollingInterval));
        });

        m_sleepingContext.wait();

        auto nextBlockHash = requestLastBlockHash();

        while (!nextBlockHash && !m_stopped)
        {
            std::this_thread::sleep_for(std::chrono::seconds(m_pollingInterval));
            nextBlockHash = requestLastBlockHash();
        }

        if (*lastBlockHash != *nextBlockHash)
        {
            break;
        }
    }

    if (m_stopped)
    {
        throw System::InterruptedException();
    }
}

void BlockchainMonitor::stop()
{
    m_stopped = true;

    m_sleepingContext.interrupt();
    m_sleepingContext.wait();
}

std::optional<Crypto::Hash> BlockchainMonitor::requestLastBlockHash()
{
    json j = {{"jsonrpc", "2.0"}, {"method", "getlastblockheader"}, {"params", {}}};

    auto res = m_httpClient->Post("/json_rpc", j.dump(), "application/json");

    if (!res)
    {
        std::cout << WarningMsg("Failed to get block hash - Is your daemon open?\n");

        return std::nullopt;
    }

    if (res->status != 200)
    {
        std::stringstream stream;

        stream << "Failed to get block hash - received unexpected http "
               << "code from server: " << res->status << std::endl;

        std::cout << WarningMsg(stream.str()) << std::endl;

        return std::nullopt;
    }

    try
    {
        json j = json::parse(res->body);

        const std::string status = j.at("result").at("status").get<std::string>();

        if (status != "OK")
        {
            std::stringstream stream;

            stream << "Failed to get block hash from daemon. Response: " << status << std::endl;

            std::cout << WarningMsg(stream.str());

            return std::nullopt;
        }

        return j.at("result").at("block_header").at("hash").get<Crypto::Hash>();
    }
    catch (const json::exception &e)
    {
        std::stringstream stream;

        stream << "Failed to parse block hash from daemon. Received data:\n"
               << res->body << "\nParse error: " << e.what() << std::endl;

        std::cout << WarningMsg(stream.str());

        return std::nullopt;
    }
}
