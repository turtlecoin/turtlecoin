// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "HttpClient.h"

#include <http/HttpParser.h>
#include <system/Ipv4Address.h>
#include <system/Ipv4Resolver.h>
#include <system/TcpConnector.h>

namespace CryptoNote
{
    HttpClient::HttpClient(System::Dispatcher &dispatcher, const std::string &address, uint16_t port):
        m_dispatcher(dispatcher),
        m_address(address),
        m_port(port)
    {
    }

    HttpClient::~HttpClient()
    {
        if (m_connected)
        {
            disconnect();
        }
    }

    void HttpClient::request(const HttpRequest &req, HttpResponse &res)
    {
        std::scoped_lock lock(m_mutex);

        if (!m_connected)
        {
            connect();
        }

        try
        {
            std::iostream stream(m_streamBuf.get());
            HttpParser parser;
            stream << req;
            stream.flush();
            parser.receiveResponse(stream, res);
        }
        catch (const std::exception &)
        {
            disconnect();
            throw;
        }
    }

    void HttpClient::connect()
    {
        try
        {
            auto ipAddr = System::Ipv4Resolver(m_dispatcher).resolve(m_address);
            m_connection = System::TcpConnector(m_dispatcher).connect(ipAddr, m_port);
            m_streamBuf.reset(new System::TcpStreambuf(m_connection));
            m_connected = true;
        }
        catch (const std::exception &e)
        {
            throw ConnectException(e.what());
        }
    }

    bool HttpClient::isConnected() const
    {
        return m_connected;
    }

    void HttpClient::disconnect()
    {
        m_streamBuf.reset();
        try
        {
            m_connection.write(nullptr, 0); // Socket shutdown.
        }
        catch (std::exception &)
        {
            // Ignoring possible exception.
        }

        try
        {
            m_connection = System::TcpConnection();
        }
        catch (std::exception &)
        {
            // Ignoring possible exception.
        }

        m_connected = false;
    }

    ConnectException::ConnectException(const std::string &whatArg): std::runtime_error(whatArg.c_str()) {}

} // namespace CryptoNote
