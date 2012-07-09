#include <cstdlib>
#include <cstdio>
#include <arpa/inet.h>
#include <sys/select.h>

#include <dns_sd.h>

#include <vector>
#include <string>

#include "zeroconf.h"

#define ORI_SERVICE_TYPE "_ori._tcp"

static DNSServiceRef registerRef;
static DNSServiceRef browseRef;

struct OriService {
    std::string name, regType, domain;
};
static std::vector<OriService> services;
static std::vector<BrowseCallback> callbacks;

void ori_deregister_mdns()
{
    printf("Stopping mDNS server\n");
    DNSServiceRefDeallocate(browseRef);
    DNSServiceRefDeallocate(registerRef);
}

// DNSServiceRegisterReply
void _register_callback(
        DNSServiceRef registerRef,
        DNSServiceFlags flags,
        DNSServiceErrorType errorCode,

        const char *name,
        const char *regtype,
        const char *domain,
        void *ctx)
{
    if (errorCode < 0) {
        printf("Error occurred while registering mDNS service\n");
        exit(1);
    }

    printf("Service registered: %s/%s/%s\n", name, regtype, domain);
}

void _resolve_callback(
        DNSServiceRef resolveSd,
        DNSServiceFlags flags,
        uint32_t interfaceIndex,
        DNSServiceErrorType errorCode,
        
        const char *fullname,
        const char *hosttarget,
        uint16_t port,
        uint16_t txtLen,
        const unsigned char *txtRecord,
        void *ctx
        )
{
    OriPeer *p = (OriPeer *)ctx;
    if (errorCode != kDNSServiceErr_NoError) {
        printf("Error resolving hostname for %s\n", fullname);
        p->port = 0;
        return;
    }

    p->hostname = hosttarget;
    p->port = ntohs(port);
}

void _browse_callback(
        DNSServiceRef browseRef,
        DNSServiceFlags flags,
        uint32_t interfaceIndex,
        DNSServiceErrorType errorCode,

        const char *serviceName,
        const char *regType,
        const char *replyDomain,
        void *ctx)
{
    if (errorCode != kDNSServiceErr_NoError) {
        printf("Error occurred while browsing mDNS services\n");
        return;
    }

    OriService service;
    service.name = serviceName;
    service.regType = regType;
    service.domain = replyDomain;
    services.push_back(service);

    OriPeer p;
    DNSServiceRef resolveSd;
    DNSServiceResolve(
            &resolveSd,
            0, 0,
            service.name.c_str(),
            service.regType.c_str(),
            service.domain.c_str(),
            _resolve_callback,
            &p);
    DNSServiceProcessResult(resolveSd);
    DNSServiceRefDeallocate(resolveSd);

    if (p.port > 0) {
        for (size_t i = 0; i < callbacks.size(); i++) {
            callbacks[i](p);
        }
    }
}

void ori_run_mdns(evutil_socket_t fd, short what, void *ctx)
{
    DNSServiceErrorType err = DNSServiceProcessResult(browseRef);
    if (err != kDNSServiceErr_NoError) {
        printf("Error processing mDNS result!\n");
        exit(1);
    }
}

struct event *MDNS_Start(uint16_t port_num, struct event_base *evbase)
{
    DNSServiceErrorType err = DNSServiceRegister(
            &registerRef,
            0, 0,
            NULL, // service name
            ORI_SERVICE_TYPE,
            NULL, // domain
            NULL, // host
            htons(port_num),
            0, NULL, // txtRecord
            _register_callback,
            NULL);
    if (err != kDNSServiceErr_NoError) {
        printf("Error setting up mDNS!\n");
        return NULL;
    }

    err = DNSServiceProcessResult(registerRef);
    if (err != kDNSServiceErr_NoError) {
        printf("Error setting up mDNS!\n");
        exit(1);
    }

    // Find other ORI services on this network
    err = DNSServiceBrowse(
            &browseRef,
            0, 0,
            ORI_SERVICE_TYPE,
            NULL, // domain,
            _browse_callback,
            NULL);
    if (err != kDNSServiceErr_NoError) {
        printf("Error browsing mDNS!\n");
        exit(1);
    }

    atexit(ori_deregister_mdns);

    int fd = DNSServiceRefSockFD(browseRef);
    if (fd < 0) {
        printf("Error getting mDNS socket\n");
        return NULL;
    }
    return event_new(evbase, fd, EV_READ | EV_PERSIST, ori_run_mdns, NULL);
}

void
MDNS_RegisterBrowseCallback(BrowseCallback cb)
{
    callbacks.push_back(cb);
}

std::vector<OriPeer> MDNS_GetPeers()
{
    std::vector<OriPeer> rval;
    for (size_t i = 0; i < services.size(); i++) {
        const OriService &s = services[i];

        OriPeer p;

        DNSServiceRef resolveSd;
        DNSServiceResolve(
                &resolveSd,
                0, 0,
                s.name.c_str(),
                s.regType.c_str(),
                s.domain.c_str(),
                _resolve_callback,
                &p);
        DNSServiceProcessResult(resolveSd);
        DNSServiceRefDeallocate(resolveSd);

        if (p.port > 0) {
            rval.push_back(p);
        }
    }

    return rval;
}

void
_print_cb(const OriPeer &peer)
{
    printf("Peer: %s:%d\n", peer.hostname.c_str(), peer.port);
}

int cmd_mdnsserver(int argc, const char *argv[])
{
    printf("Starting mDNS server...\n");
    struct event_base *evbase = event_base_new();
    struct event *mdns_event = MDNS_Start(rand() % 1000 + 1000, evbase);
    event_add(mdns_event, NULL);

    MDNS_RegisterBrowseCallback(_print_cb);

    event_base_dispatch(evbase);
    return 0;
}
