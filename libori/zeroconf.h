#ifndef __ZEROCONF_H__
#define __ZEROCONF_H__

struct OriPeer
{
    std::string hostname;
    uint16_t port;
};

#include <event2/event.h>

/**
 * @returns the mDNS callback event, which must be made pending on evbase for
 * mDNS to run properly
 */
struct event *MDNS_Start(uint16_t port_num, struct event_base *evbase);

typedef void (*BrowseCallback)(const OriPeer &peer);
void MDNS_RegisterBrowseCallback(BrowseCallback cb);
std::vector<OriPeer> MDNS_GetPeers();

#endif
