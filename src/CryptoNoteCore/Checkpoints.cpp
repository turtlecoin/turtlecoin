// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "Checkpoints.h"
#include "Common/StringTools.h"
#include <boost/regex.hpp>

using namespace Logging;

namespace CryptoNote
{
//---------------------------------------------------------------------------
    Checkpoints::Checkpoints(Logging::ILogger &log) : logger(log, "checkpoints")
    {}

//---------------------------------------------------------------------------
    bool Checkpoints::addCheckpoint(uint32_t index, const std::string &hash_str)
    {
        Crypto::Hash h = NULL_HASH;

        if (!Common::podFromHex(hash_str, h))
        {
            logger(ERROR, BRIGHT_RED) << "INVALID HASH IN CHECKPOINTS!";
            return false;
        }

        if (!(0 == points.count(index)))
        {
            logger(ERROR, BRIGHT_RED) << "CHECKPOINT ALREADY EXISTS!";
            return false;
        }

        points[index] = h;
        return true;
    }

//---------------------------------------------------------------------------
    const boost::regex linesregx("\\r\\n|\\n\\r|\\n|\\r");
    const boost::regex fieldsregx(",(?=(?:[^\"]*\"[^\"]*\")*(?![^\"]*\"))");

    bool Checkpoints::loadCheckpointsFromFile(const std::string &fileName)
    {
        std::string buff;
        if (!Common::loadFileToString(fileName, buff))
        {
            logger(ERROR, BRIGHT_RED) << "Could not load checkpoints file: " << fileName;
            return false;
        }
        const char *data = buff.c_str();
        unsigned int length = strlen(data);

        boost::cregex_token_iterator li(data, data + length, linesregx, -1);
        boost::cregex_token_iterator end;

        int count = 0;
        while (li != end)
        {
            std::string line = li->str();
            ++li;

            boost::sregex_token_iterator ti(line.begin(), line.end(), fieldsregx, -1);
            boost::sregex_token_iterator end2;

            std::vector<std::string> row;
            while (ti != end2)
            {
                std::string token = ti->str();
                ++ti;
                row.push_back(token);
            }
            if (row.size() != 2)
            {
                logger(ERROR, BRIGHT_RED) << "Invalid checkpoint file format";
                return false;
            } else
            {
                uint32_t height = stoi(row[0]);
                bool r = addCheckpoint(height, row[1]);
                if (!r)
                {
                    return false;
                }
                count += 1;
            }
        }

        logger(INFO) << "Loaded " << count << " checkpoints from " << fileName;
        return true;
    }

//---------------------------------------------------------------------------
    bool Checkpoints::isInCheckpointZone(uint32_t index) const
    {
        return !points.empty() && (index <= (--points.end())->first);
    }

//---------------------------------------------------------------------------
    bool Checkpoints::checkBlock(uint32_t index, const Crypto::Hash &h,
                                 bool &isCheckpoint) const
    {
        auto it = points.find(index);
        isCheckpoint = it != points.end();
        if (!isCheckpoint)
            return true;

        if (it->second == h)
        {
            if (index % 100 == 0)
            {
                logger(Logging::INFO, BRIGHT_GREEN)
                        << "CHECKPOINT PASSED FOR INDEX " << index << " " << h;
            }
            return true;
        } else
        {
            logger(Logging::WARNING, BRIGHT_YELLOW) << "CHECKPOINT FAILED FOR HEIGHT " << index
                                                    << ". EXPECTED HASH: " << it->second
                                                    << ", FETCHED HASH: " << h;
            return false;
        }
    }

//---------------------------------------------------------------------------
    bool Checkpoints::checkBlock(uint32_t index, const Crypto::Hash &h) const
    {
        bool ignored;
        return checkBlock(index, h, ignored);
    }

//---------------------------------------------------------------------------
    bool Checkpoints::isAlternativeBlockAllowed(uint32_t blockchainSize,
                                                uint32_t blockIndex) const
    {
        if (blockchainSize == 0)
        {
            return false;
        }

        auto it = points.upper_bound(blockchainSize);
        // Is blockchainSize before the first checkpoint?
        if (it == points.begin())
        {
            return true;
        }

        --it;
        uint32_t checkpointIndex = it->first;
        return checkpointIndex < blockIndex;
    }

    std::vector<uint32_t> Checkpoints::getCheckpointHeights() const
    {
        std::vector<uint32_t> checkpointHeights;
        checkpointHeights.reserve(points.size());
        for (const auto &it : points)
        {
            checkpointHeights.push_back(it.first);
        }

        return checkpointHeights;
    }

}
