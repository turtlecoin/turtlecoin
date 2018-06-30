// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include <future>
#include <INode.h>

namespace Tests
{

    class NodeCallback
    {
    public:
        CryptoNote::INode::Callback callback()
        {
            prom = std::promise<std::error_code>(); // reset std::promise
            result = prom.get_future();
            return [this](std::error_code ec)
            {
                std::promise<std::error_code> localPromise(std::move(prom));
                localPromise.set_value(ec);
            };
        }

        std::error_code get()
        {
            return result.get();
        }

    private:
        std::promise<std::error_code> prom;
        std::future<std::error_code> result;
    };

}
