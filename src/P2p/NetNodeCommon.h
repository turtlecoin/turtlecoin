// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <boost/uuid/uuid.hpp>

#include "CryptoNote.h"
#include "P2pProtocolTypes.h"

namespace CryptoNote {

  struct CryptoNoteConnectionContext;

  struct IP2pEndpoint {
    virtual void relay_notify_to_all(int command, const BinaryArray& data_buff, const boost::uuids::uuid* excludeConnection) = 0;
    virtual bool invoke_notify_to_peer(int command, const BinaryArray& req_buff, const CryptoNote::CryptoNoteConnectionContext& context) = 0;
    virtual uint64_t get_connections_count()=0;
    virtual void for_each_connection(std::function<void(CryptoNote::CryptoNoteConnectionContext&, uint64_t)> f) = 0;
    // can be called from external threads
    virtual void externalRelayNotifyToAll(int command, const BinaryArray& data_buff, const boost::uuids::uuid* excludeConnection) = 0;
    virtual void externalRelayNotifyToList(int command, const BinaryArray& data_buff, const std::list<boost::uuids::uuid> relayList) = 0;
  };

  struct p2p_endpoint_stub: public IP2pEndpoint {
    virtual void relay_notify_to_all(int command, const BinaryArray& data_buff, const boost::uuids::uuid* excludeConnection) override {}
    virtual bool invoke_notify_to_peer(int command, const BinaryArray& req_buff, const CryptoNote::CryptoNoteConnectionContext& context) override { return true; }
    virtual void for_each_connection(std::function<void(CryptoNote::CryptoNoteConnectionContext&, uint64_t)> f) override {}
    virtual uint64_t get_connections_count() override { return 0; }   
    virtual void externalRelayNotifyToAll(int command, const BinaryArray& data_buff, const boost::uuids::uuid* excludeConnection) override {}
    virtual void externalRelayNotifyToList(int command, const BinaryArray& data_buff, const std::list<boost::uuids::uuid> relayList) override {}
  };
}
