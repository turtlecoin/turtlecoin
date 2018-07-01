#include "TransferCommand.h"

using namespace Logging;

namespace CryptoNote
{
    TransferCommand::TransferCommand(const CryptoNote::Currency &currency) :
            m_currency(currency), fake_outs_count(0), fee(currency.minimumFee())
    {
    }

    bool TransferCommand::parseArguments(LoggerRef &logger, const std::vector<std::string> &args)
    {

        CryptoNote::ArgumentReader<std::vector<std::string>::const_iterator> ar(args.begin(), args.end());

        try
        {

            auto mixin_str = ar.next();

            if (!Common::fromString(mixin_str, fake_outs_count))
            {
                logger(ERROR, BRIGHT_RED) << "mixin_count should be non-negative integer, got " << mixin_str;
                return false;
            }

            while (!ar.eof())
            {

                auto arg = ar.next();

                if (arg.size() && arg[0] == '-')
                {

                    const auto &value = ar.next();

                    if (arg == "-p")
                    {
                        if (!createTxExtraWithPaymentId(value, extra))
                        {
                            logger(ERROR, BRIGHT_RED) << "payment ID has invalid format: \"" << value
                                                      << "\", expected 64-character string";
                            return false;
                        }
                    } else if (arg == "-f")
                    {
                        bool ok = m_currency.parseAmount(value, fee);
                        if (!ok)
                        {
                            logger(ERROR, BRIGHT_RED) << "Fee value is invalid: " << value;
                            return false;
                        }

                        if (fee < m_currency.minimumFee())
                        {
                            logger(ERROR, BRIGHT_RED) << "Fee value is less than minimum: " << m_currency.minimumFee();
                            return false;
                        }
                    }
                } else
                {
                    WalletLegacyTransfer destination;
                    CryptoNote::TransactionDestinationEntry de;

                    if (!m_currency.parseAccountAddressString(arg, de.addr))
                    {
                        Crypto::Hash paymentId;
                        if (CryptoNote::parsePaymentId(arg, paymentId))
                        {
                            logger(ERROR, BRIGHT_RED)
                                    << "Invalid payment ID usage. Please, use -p <payment_id>. See help for details.";
                        } else
                        {
                            logger(ERROR, BRIGHT_RED) << "Wrong address: " << arg;
                        }

                        return false;
                    }

                    auto value = ar.next();
                    bool ok = m_currency.parseAmount(value, de.amount);
                    if (!ok || 0 == de.amount)
                    {
                        logger(ERROR, BRIGHT_RED) << "amount is wrong: " << arg << ' ' << value <<
                                                  ", expected number from 0 to "
                                                  << m_currency.formatAmount(std::numeric_limits<uint64_t>::max());
                        return false;
                    }
                    destination.address = arg;
                    destination.amount = de.amount;

                    dsts.push_back(destination);
                }
            }

            if (dsts.empty())
            {
                logger(ERROR, BRIGHT_RED) << "At least one destination address is required";
                return false;
            }
        } catch (const std::exception &e)
        {
            logger(ERROR, BRIGHT_RED) << e.what();
            return false;
        }

        return true;
    }
}
