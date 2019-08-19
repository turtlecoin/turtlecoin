// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <string>
#include <system_error>

namespace CryptoNote
{
    namespace NodeError
    {
        // custom error conditions enum type:
        enum NodeErrorCodes
        {
            NOT_INITIALIZED = 1,
            ALREADY_INITIALIZED,
            NETWORK_ERROR,
            NODE_BUSY,
            INTERNAL_NODE_ERROR,
            REQUEST_ERROR,
            CONNECT_ERROR,
            TIMEOUT
        };

        // custom category:
        class NodeErrorCategory : public std::error_category
        {
          public:
            static NodeErrorCategory INSTANCE;

            virtual const char *name() const throw() override
            {
                return "NodeErrorCategory";
            }

            virtual std::error_condition default_error_condition(int ev) const throw() override
            {
                return std::error_condition(ev, *this);
            }

            virtual std::string message(int ev) const override
            {
                switch (ev)
                {
                    case NOT_INITIALIZED:
                        return "Object was not initialized";
                    case ALREADY_INITIALIZED:
                        return "Object has been already initialized";
                    case NETWORK_ERROR:
                        return "Network error";
                    case NODE_BUSY:
                        return "Node is busy";
                    case INTERNAL_NODE_ERROR:
                        return "Internal node error";
                    case REQUEST_ERROR:
                        return "Error in request parameters";
                    case CONNECT_ERROR:
                        return "Can't connect to daemon";
                    case TIMEOUT:
                        return "Operation timed out";
                    default:
                        return "Unknown error";
                }
            }

          private:
            NodeErrorCategory() {}
        };

    } // namespace NodeError
} // namespace CryptoNote

inline std::error_code make_error_code(CryptoNote::NodeError::NodeErrorCodes e)
{
    return std::error_code(static_cast<int>(e), CryptoNote::NodeError::NodeErrorCategory::INSTANCE);
}
