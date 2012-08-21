#ifndef __ZEROCONF_H__
#define __ZEROCONF_H__

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

#endif
