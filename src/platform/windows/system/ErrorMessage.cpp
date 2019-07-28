// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "ErrorMessage.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <cstddef>
#include <windows.h>

namespace System
{
    std::string lastErrorMessage()
    {
        return errorMessage(GetLastError());
    }

    std::string errorMessage(int error)
    {
        struct Buffer
        {
            ~Buffer()
            {
                if (pointer != nullptr)
                {
                    LocalFree(pointer);
                }
            }

            LPTSTR pointer = nullptr;
        } buffer;

        auto size = FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
            nullptr,
            error,
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
            reinterpret_cast<LPTSTR>(&buffer.pointer),
            0,
            nullptr);
        return "result=" + std::to_string(error) + ", " + std::string(buffer.pointer, size);
    }

} // namespace System
