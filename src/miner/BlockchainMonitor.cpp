// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2020, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "BlockchainMonitor.h"

#include "common/StringTools.h"

#include <system/EventLock.h>
#include <system/InterruptedException.h>
#include <system/Timer.h>
#include <utilities/ColouredMsg.h>
#include <version.h>
#include <iostream>

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
    std::stringstream userAgent;
    userAgent << "SoloMiner/" << PROJECT_VERSION_LONG;

    m_requestHeaders = {{"User-Agent", userAgent.str()}};
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
    auto res = m_httpClient->Get("/block/last", m_requestHeaders);

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


    rapidjson::Document jsonBody;

    if (jsonBody.Parse(res->body.c_str()).HasParseError())
    {
        std::stringstream stream;

        stream << "Failed to parse block hash from daemon. Received data:\n"
               << res->body << std::endl;

        std::cout << WarningMsg(stream.str());

        return std::nullopt;
    }

    Crypto::Hash hash;

    hash.fromJSON(getJsonValue(jsonBody, "hash"));

    return hash;
}
