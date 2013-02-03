/*
 * Copyright (c) 2012 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __ORI_ZEROCONF_H__
#define __ORI_ZEROCONF_H__

struct OriPeer
{
    std::string hostname;
    uint16_t port;
};

#include <event2/event.h>

void MDNS_Register(uint16_t port_num);

/**
 * @returns the mDNS callback event, which must be made pending on evbase for
 * mDNS to run properly
 */
struct event *MDNS_Browse(struct event_base *evbase);

#include <boost/function.hpp>
//typedef void (*BrowseCallback)(const OriPeer &peer);
typedef boost::function<void (const OriPeer &peer)> BrowseCallback;
void MDNS_RegisterBrowseCallback(BrowseCallback cb);
std::vector<OriPeer> MDNS_GetPeers();

#endif /* __ORI_ZEROCONF_H__ */

