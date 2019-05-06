// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "BlockchainMonitor.h"

#include "Common/StringTools.h"

#include <System/EventLock.h>
#include <System/Timer.h>
#include <System/InterruptedException.h>

#include "Rpc/CoreRpcServerCommandsDefinitions.h"
#include "Rpc/JsonRpc.h"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"

#include <Utilities/ColouredMsg.h>

BlockchainMonitor::BlockchainMonitor(
    System::Dispatcher& dispatcher,
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

    while (!lastBlockHash && !m_stopped) {
        std::this_thread::sleep_for(std::chrono::seconds(m_pollingInterval));
        lastBlockHash = requestLastBlockHash();
    }

    while(!m_stopped)
    {
        m_sleepingContext.spawn([this] ()
        {
            System::Timer timer(m_dispatcher);
            timer.sleep(std::chrono::seconds(m_pollingInterval));
        });

        m_sleepingContext.wait();

        auto nextBlockHash = requestLastBlockHash();

        while (!nextBlockHash && !m_stopped) {
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
    rapidjson::StringBuffer string_buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(string_buffer);

    writer.StartObject();
    writer.Key("jsonrpc");
    writer.String("2.0");
    writer.Key("method");
    writer.String("getlastblockheader");
    writer.Key("params");
    writer.Null();
    writer.EndObject();

    auto res = m_httpClient->Post("/json_rpc", string_buffer.GetString(), "application/json");

    if (!res)
    {
        std::cout << WarningMsg("Failed to get block hash - Is your daemon open?\n");

        return std::nullopt;
    }

    if (res->status != 200)
    {
        std::stringstream stream;

        stream << "Failed to get block hash - received unexpected http "
               << "code from server: "
               << res->status << std::endl;

        std::cout << WarningMsg(stream.str()) << std::endl;

        return std::nullopt;
    }

    std::cout << "befure parse blockchain monitor" << std::endl;
    try {
        rapidjson::Document j;
        j.Parse(res->body);
        if (j.HasParseError())
            throw JsonException(GetParseError_En(j.GetParseError()));
        const std::string status = getStringFromJSON(getJsonValue(j, "result"), "status");

        if (status != "OK") {
            std::stringstream stream;
            stream << "Failed to get block hash from daemon. Response: "
                << status << std::endl;
            std::cout << WarningMsg(stream.str());

            return std::nullopt;
        }

        Crypto::Hash hash;
        hash.fromString(getStringFromJSON(getJsonValue(getJsonValue(j, "result"), "block_header"), "hash"));
        return hash;    
    } catch (const JsonException &e) {
        std::stringstream stream;
        stream << "Failed to parse block hash from daemon. Received data:\n"
            << res->body << "\nParse error: " << e.what()
            << std::endl;
        
        std::cout << WarningMsg(stream.str());
        return std::nullopt;
    }
    
}
