// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.
#include "ConsoleHandler.h"

#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#else
#include <stdio.h>
#include <unistd.h>
#endif

#include <boost/algorithm/string.hpp>

using Common::Console::Color;

namespace Common
{
    /////////////////////////////////////////////////////////////////////////////
    // AsyncConsoleReader
    /////////////////////////////////////////////////////////////////////////////
    AsyncConsoleReader::AsyncConsoleReader(): m_stop(true) {}

    AsyncConsoleReader::~AsyncConsoleReader()
    {
        stop();
    }

    void AsyncConsoleReader::start()
    {
        m_stop = false;
        m_thread = std::thread(std::bind(&AsyncConsoleReader::consoleThread, this));
    }

    bool AsyncConsoleReader::getline(std::string &line)
    {
        return m_queue.pop(line);
    }

    void AsyncConsoleReader::pause()
    {
        if (m_stop)
        {
            return;
        }

        m_stop = true;

        if (m_thread.joinable())
        {
            m_thread.join();
        }

        m_thread = std::thread();
    }

    void AsyncConsoleReader::unpause()
    {
        start();
    }

    void AsyncConsoleReader::stop()
    {
        if (m_stop)
        {
            return; // already stopping/stopped
        }

        m_stop = true;
        m_queue.close();
#ifdef _WIN32
        ::CloseHandle(::GetStdHandle(STD_INPUT_HANDLE));
#endif

        if (m_thread.joinable())
        {
            m_thread.join();
        }

        m_thread = std::thread();
    }

    bool AsyncConsoleReader::stopped() const
    {
        return m_stop;
    }

    void AsyncConsoleReader::consoleThread()
    {
        while (waitInput())
        {
            std::string line;

            if (!std::getline(std::cin, line))
            {
                break;
            }

            if (!m_queue.push(line))
            {
                break;
            }
        }
    }

    bool AsyncConsoleReader::waitInput()
    {
#ifndef _WIN32
        int stdin_fileno = ::fileno(stdin);

        while (!m_stop)
        {
            fd_set read_set;
            FD_ZERO(&read_set);
            FD_SET(stdin_fileno, &read_set);

            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 100 * 1000;

            int retval = ::select(stdin_fileno + 1, &read_set, NULL, NULL, &tv);

            if (retval == -1 && errno == EINTR)
            {
                continue;
            }

            if (retval < 0)
            {
                return false;
            }

            if (retval > 0)
            {
                return true;
            }
        }
#endif

        return !m_stop;
    }

    /////////////////////////////////////////////////////////////////////////////
    // ConsoleHandler
    /////////////////////////////////////////////////////////////////////////////
    ConsoleHandler::~ConsoleHandler()
    {
        stop();
    }

    void ConsoleHandler::start(bool startThread, const std::string &prompt, Console::Color promptColor)
    {
        m_prompt = prompt;
        m_promptColor = promptColor;
        m_consoleReader.start();

        if (startThread)
        {
            m_thread = std::thread(std::bind(&ConsoleHandler::handlerThread, this));
        }
        else
        {
            handlerThread();
        }
    }

    void ConsoleHandler::stop()
    {
        requestStop();
        wait();
    }

    void ConsoleHandler::pause()
    {
        m_consoleReader.pause();
    }

    void ConsoleHandler::unpause()
    {
        m_consoleReader.unpause();
    }

    void ConsoleHandler::wait()
    {
        try
        {
            if (m_thread.joinable())
            {
                m_thread.join();
            }
        }
        catch (std::exception &e)
        {
            std::cerr << "Exception in ConsoleHandler::wait - " << e.what() << std::endl;
        }
    }

    void ConsoleHandler::requestStop()
    {
        m_consoleReader.stop();
    }

    std::string ConsoleHandler::getUsage() const
    {
        if (m_handlers.empty())
        {
            return std::string();
        }

        std::stringstream ss;

        uint64_t maxlen = std::max_element(
                              m_handlers.begin(),
                              m_handlers.end(),
                              [](CommandHandlersMap::const_reference &a, CommandHandlersMap::const_reference &b) {
                                  return a.first.size() < b.first.size();
                              })
                              ->first.size();

        for (auto &x : m_handlers)
        {
            ss << std::left << std::setw(maxlen + 3) << x.first << x.second.second << std::endl;
        }

        return ss.str();
    }

    void ConsoleHandler::setHandler(
        const std::string &command,
        const ConsoleCommandHandler &handler,
        const std::string &usage)
    {
        m_handlers[command] = std::make_pair(handler, usage);
    }

    bool ConsoleHandler::runCommand(const std::vector<std::string> &cmdAndArgs)
    {
        if (cmdAndArgs.size() == 0)
        {
            return false;
        }

        const auto &cmd = cmdAndArgs.front();
        auto hIter = m_handlers.find(cmd);

        if (hIter == m_handlers.end())
        {
            std::cout << "Unknown command: " << cmd << std::endl;
            return false;
        }

        std::vector<std::string> args(cmdAndArgs.begin() + 1, cmdAndArgs.end());
        hIter->second.first(args);
        return true;
    }

    void ConsoleHandler::handleCommand(const std::string &cmd)
    {
        std::vector<std::string> args;
        boost::split(args, cmd, boost::is_any_of(" "), boost::token_compress_on);
        runCommand(args);
    }

    void ConsoleHandler::handlerThread()
    {
        std::string line;

        while (!m_consoleReader.stopped())
        {
            try
            {
                if (!m_prompt.empty())
                {
                    if (m_promptColor != Color::Default)
                    {
                        Console::setTextColor(m_promptColor);
                    }

                    std::cout << m_prompt;
                    std::cout.flush();

                    if (m_promptColor != Color::Default)
                    {
                        Console::setTextColor(Color::Default);
                    }
                }

                if (!m_consoleReader.getline(line))
                {
                    break;
                }

                boost::algorithm::trim(line);
                if (!line.empty())
                {
                    handleCommand(line);
                }
            }
            catch (std::exception &)
            {
                // ignore errors
            }
        }
    }
} // namespace Common
