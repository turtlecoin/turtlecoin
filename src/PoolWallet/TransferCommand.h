#pragma once

#include "CryptoNoteCore/Currency.h"
#include "WalletLegacy/WalletLegacy.h"
#include "Common/StringTools.h"

#include <Logging/LoggerRef.h>

using namespace Logging;

namespace CryptoNote
{
    class TransferCommand
    {
    public:
        const CryptoNote::Currency &m_currency;
        size_t fake_outs_count;
        std::vector<CryptoNote::WalletLegacyTransfer> dsts;
        std::vector<uint8_t> extra;
        uint64_t fee;

        TransferCommand(const CryptoNote::Currency &currency);

        bool parseArguments(LoggerRef &logger, const std::vector<std::string> &args);
    };

    template<typename IterT, typename ValueT = typename IterT::value_type>
    class ArgumentReader
    {
    public:

        ArgumentReader(IterT begin, IterT end) :
                m_begin(begin), m_end(end), m_cur(begin)
        {
        }

        bool eof() const
        {
            return m_cur == m_end;
        }

        ValueT next()
        {
            if (eof())
            {
                throw std::runtime_error("unexpected end of arguments");
            }

            return *m_cur++;
        }

    private:

        IterT m_cur;
        IterT m_begin;
        IterT m_end;
    };
}
