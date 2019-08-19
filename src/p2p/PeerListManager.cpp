// Copyright (c) 2018, The TurtleCoin Developers
//
// Please see the included LICENSE file for more information.

#include "PeerListManager.h"

#include "serialization/SerializationOverloads.h"

#include <algorithm>
#include <system/Ipv4Address.h>
#include <time.h>

void PeerlistManager::serialize(CryptoNote::ISerializer &s)
{
    const uint8_t currentVersion = 1;
    uint8_t version = currentVersion;

    s(version, "version");

    if (version != currentVersion)
    {
        return;
    }

    s(m_peers_white, "whitelist");
    s(m_peers_gray, "graylist");
}

void serialize(NetworkAddress &na, CryptoNote::ISerializer &s)
{
    s(na.ip, "ip");
    s(na.port, "port");
}

void serialize(PeerlistEntry &pe, CryptoNote::ISerializer &s)
{
    s(pe.adr, "adr");
    s(pe.id, "id");
    s(pe.last_seen, "last_seen");
}

PeerlistManager::PeerlistManager():
    m_whitePeerlist(m_peers_white, CryptoNote::P2P_LOCAL_WHITE_PEERLIST_LIMIT),
    m_grayPeerlist(m_peers_gray, CryptoNote::P2P_LOCAL_GRAY_PEERLIST_LIMIT)
{
}

bool PeerlistManager::init(bool allow_local_ip)
{
    m_allow_local_ip = allow_local_ip;
    return true;
}

void PeerlistManager::trim_white_peerlist()
{
    m_whitePeerlist.trim();
}

void PeerlistManager::trim_gray_peerlist()
{
    m_grayPeerlist.trim();
}

bool PeerlistManager::merge_peerlist(const std::list<PeerlistEntry> &outer_bs)
{
    for (const PeerlistEntry &be : outer_bs)
    {
        append_with_peer_gray(be);
    }

    // delete extra elements
    trim_gray_peerlist();
    return true;
}

bool PeerlistManager::get_white_peer_by_index(PeerlistEntry &p, size_t i) const
{
    return m_whitePeerlist.get(p, i);
}

bool PeerlistManager::get_gray_peer_by_index(PeerlistEntry &p, size_t i) const
{
    return m_grayPeerlist.get(p, i);
}

bool PeerlistManager::is_ip_allowed(uint32_t ip) const
{
    System::Ipv4Address addr(networkToHost(ip));

    // never allow loopback ip
    if (addr.isLoopback())
    {
        return false;
    }

    if (!m_allow_local_ip && addr.isPrivate())
    {
        return false;
    }

    return true;
}

bool PeerlistManager::get_peerlist_head(std::list<PeerlistEntry> &bs_head, uint32_t depth)
{
    /* Sort the peers by last seen [Newer peers come first] */
    std::sort(m_peers_white.begin(), m_peers_white.end(), [](const auto &lhs, const auto &rhs) {
        return lhs.last_seen > rhs.last_seen;
    });

    uint32_t i = 0;

    for (const auto &peer : m_peers_white)
    {
        if (!peer.last_seen)
        {
            continue;
        }

        bs_head.push_back(peer);

        if (i > depth)
        {
            break;
        }

        i++;
    }

    return true;
}

bool PeerlistManager::get_peerlist_full(std::list<PeerlistEntry> &pl_gray, std::list<PeerlistEntry> &pl_white) const
{
    std::copy(m_peers_gray.begin(), m_peers_gray.end(), std::back_inserter(pl_gray));
    std::copy(m_peers_white.begin(), m_peers_white.end(), std::back_inserter(pl_white));

    return true;
}

bool PeerlistManager::set_peer_just_seen(uint64_t peer, uint32_t ip, uint32_t port)
{
    NetworkAddress addr;
    addr.ip = ip;
    addr.port = port;
    return set_peer_just_seen(peer, addr);
}

bool PeerlistManager::set_peer_just_seen(uint64_t peer, const NetworkAddress &addr)
{
    try
    {
        // find in white list
        PeerlistEntry ple;
        ple.adr = addr;
        ple.id = peer;
        ple.last_seen = time(NULL);
        return append_with_peer_white(ple);
    }
    catch (std::exception &)
    {
    }

    return false;
}

bool PeerlistManager::append_with_peer_white(const PeerlistEntry &newPeer)
{
    try
    {
        if (!is_ip_allowed(newPeer.adr.ip))
        {
            return true;
        }

        /* See if the peer already exists */
        auto whiteListIterator = std::find_if(m_peers_white.begin(), m_peers_white.end(), [&newPeer](const auto peer) {
            return peer.adr == newPeer.adr;
        });

        /* Peer doesn't exist */
        if (whiteListIterator == m_peers_white.end())
        {
            // put new record into white list
            m_peers_white.push_back(newPeer);
            trim_white_peerlist();
        }
        /* It does, update it */
        else
        {
            // update record in white list
            *whiteListIterator = newPeer;
        }

        // remove from gray list, if need
        auto grayListIterator = std::find_if(
            m_peers_gray.begin(), m_peers_gray.end(), [&newPeer](const auto peer) { return peer.adr == newPeer.adr; });

        if (grayListIterator != m_peers_gray.end())
        {
            m_peers_gray.erase(grayListIterator);
        }

        return true;
    }
    catch (std::exception &)
    {
        return false;
    }

    return false;
}

bool PeerlistManager::append_with_peer_gray(const PeerlistEntry &newPeer)
{
    try
    {
        if (!is_ip_allowed(newPeer.adr.ip))
        {
            return true;
        }

        // find in white list
        auto whiteListIterator = std::find_if(m_peers_white.begin(), m_peers_white.end(), [&newPeer](const auto peer) {
            return peer.adr == newPeer.adr;
        });

        if (whiteListIterator != m_peers_white.end())
        {
            return true;
        }

        // update gray list
        auto grayListIterator = std::find_if(
            m_peers_gray.begin(), m_peers_gray.end(), [&newPeer](const auto peer) { return peer.adr == newPeer.adr; });

        if (grayListIterator == m_peers_gray.end())
        {
            // put new record into white list
            m_peers_gray.push_back(newPeer);
            trim_gray_peerlist();
        }
        else
        {
            // update record in white list
            *grayListIterator = newPeer;
        }

        return true;
    }
    catch (std::exception &)
    {
        return false;
    }

    return false;
}

Peerlist &PeerlistManager::getWhite()
{
    return m_whitePeerlist;
}

Peerlist &PeerlistManager::getGray()
{
    return m_grayPeerlist;
}
