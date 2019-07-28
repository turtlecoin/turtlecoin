// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <map>
#include <ostream>
#include <string>

namespace CryptoNote
{
    class HttpResponse
    {
      public:
        enum HTTP_STATUS
        {
            STATUS_200,
            STATUS_404,
            STATUS_500
        };

        HttpResponse();

        void setStatus(HTTP_STATUS s);

        void addHeader(const std::string &name, const std::string &value);

        void setBody(const std::string &b);

        const std::map<std::string, std::string> &getHeaders() const
        {
            return headers;
        }

        HTTP_STATUS getStatus() const
        {
            return status;
        }

        const std::string &getBody() const
        {
            return body;
        }

      private:
        friend std::ostream &operator<<(std::ostream &os, const HttpResponse &resp);

        std::ostream &printHttpResponse(std::ostream &os) const;

        HTTP_STATUS status;

        std::map<std::string, std::string> headers;

        std::string body;
    };

    inline std::ostream &operator<<(std::ostream &os, const HttpResponse &resp)
    {
        return resp.printHttpResponse(os);
    }

} // namespace CryptoNote
