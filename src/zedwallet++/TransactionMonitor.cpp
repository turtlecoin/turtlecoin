// Copyright (c) 2018-2019, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

///////////////////////////////////////////
#include <zedwallet++/TransactionMonitor.h>
///////////////////////////////////////////

#include <iostream>
#include <utilities/ColouredMsg.h>
#include <zedwallet++/CommandImplementations.h>
#include <zedwallet++/GetInput.h>

void TransactionMonitor::start()
{
    /* Grab new transactions and push them into a queue for processing */
    m_walletBackend->m_eventHandler->onTransaction.subscribe([this](const auto tx) { m_queuedTransactions.push(tx); });

    const std::string prompt = getPrompt(m_walletBackend);

    while (!m_shouldStop)
    {
        const auto tx = m_queuedTransactions.peek();

        /* Make sure we're not printing a garbage tx */
        if (m_shouldStop)
        {
            break;
        }

        /* Don't print out fusion or outgoing transactions */
        if (!tx.isFusionTransaction() && tx.totalAmount() > 0)
        {
            /* Aquire the lock, so we're not interleaving our output when a
               command is being handled, for example, transferring */
            std::scoped_lock lock(*m_mutex);

            std::cout << InformationMsg("\nNew transaction found!\n\n");

            printIncomingTransfer(tx);

            /* Write out the prompt after every transfer. This prevents the
               wallet being in a 'ready' state, waiting for input, but looking
               like it's not. */
            std::cout << InformationMsg(prompt) << std::flush;
        }

        m_queuedTransactions.deleteFront();
    }

    m_walletBackend->m_eventHandler->onTransaction.unsubscribe();
}

void TransactionMonitor::stop()
{
    m_shouldStop = true;
    m_queuedTransactions.stop();
}

std::shared_ptr<std::mutex> TransactionMonitor::getMutex() const
{
    return m_mutex;
}
