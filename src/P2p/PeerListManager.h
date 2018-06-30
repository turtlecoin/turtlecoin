// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero Project
// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <list>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>

#include "P2pProtocolTypes.h"
#include "CryptoNoteConfig.h"

namespace CryptoNote
{

    class ISerializer;
/************************************************************************/
/*                                                                      */
/************************************************************************/
    class PeerlistManager
    {
        struct by_time
        {
        };
        struct by_id
        {
        };
        struct by_addr
        {
        };

        typedef boost::multi_index_container<
                PeerlistEntry,
                boost::multi_index::indexed_by<
                        // access by peerlist_entry::net_adress
                        boost::multi_index::ordered_unique<boost::multi_index::tag<by_addr>, boost::multi_index::member<PeerlistEntry, NetworkAddress, &PeerlistEntry::adr> >,
                        // sort by peerlist_entry::last_seen<
                        boost::multi_index::ordered_non_unique<boost::multi_index::tag<by_time>, boost::multi_index::member<PeerlistEntry, uint64_t, &PeerlistEntry::last_seen> >
                >
        > peers_indexed;

    public:

        class Peerlist
        {
        public:
            Peerlist(peers_indexed &peers, size_t maxSize);

            size_t count() const;

            bool get(PeerlistEntry &entry, size_t index) const;

            void trim();

        private:
            peers_indexed &m_peers;
            const size_t m_maxSize;
        };

        PeerlistManager();

        bool init(bool allow_local_ip);

        size_t get_white_peers_count() const
        { return m_peers_white.size(); }

        size_t get_gray_peers_count() const
        { return m_peers_gray.size(); }

        bool merge_peerlist(const std::list<PeerlistEntry> &outer_bs);

        bool get_peerlist_head(std::list<PeerlistEntry> &bs_head,
                               uint32_t depth = CryptoNote::P2P_DEFAULT_PEERS_IN_HANDSHAKE) const;

        bool get_peerlist_full(std::list<PeerlistEntry> &pl_gray, std::list<PeerlistEntry> &pl_white) const;

        bool get_white_peer_by_index(PeerlistEntry &p, size_t i) const;

        bool get_gray_peer_by_index(PeerlistEntry &p, size_t i) const;

        bool append_with_peer_white(const PeerlistEntry &pr);

        bool append_with_peer_gray(const PeerlistEntry &pr);

        bool set_peer_just_seen(PeerIdType peer, uint32_t ip, uint32_t port);

        bool set_peer_just_seen(PeerIdType peer, const NetworkAddress &addr);

        bool set_peer_unreachable(const PeerlistEntry &pr);

        bool is_ip_allowed(uint32_t ip) const;

        void trim_white_peerlist();

        void trim_gray_peerlist();

        void serialize(ISerializer &s);

        Peerlist &getWhite();

        Peerlist &getGray();

    private:
        std::string m_config_folder;
        bool m_allow_local_ip;
        peers_indexed m_peers_gray;
        peers_indexed m_peers_white;
        Peerlist m_whitePeerlist;
        Peerlist m_grayPeerlist;
    };

}
