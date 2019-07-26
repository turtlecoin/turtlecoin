// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#ifndef HTTPPARSER_H_
#define HTTPPARSER_H_

#include <iostream>
#include <map>
#include <string>
#include "HttpRequest.h"
#include "HttpResponse.h"

namespace CryptoNote
{

//Blocking HttpParser
    class HttpParser
    {
    public:
        HttpParser()
        {};

        static void receiveRequest(std::istream &stream, HttpRequest &request);

        static void receiveResponse(std::istream &stream, HttpResponse &response);

        static HttpResponse::HTTP_STATUS parseResponseStatusFromString(const std::string &status);

    private:
        static void readWord(std::istream &stream, std::string &word);

        static void readHeaders(std::istream &stream, HttpRequest::Headers &headers);

        static bool readHeader(std::istream &stream, std::string &name, std::string &value);

        static size_t getBodyLen(const HttpRequest::Headers &headers);

        static void readBody(std::istream &stream, std::string &body, const size_t bodyLen);
    };

} //namespace CryptoNote

#endif /* HTTPPARSER_H_ */
