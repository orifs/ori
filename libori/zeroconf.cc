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

    // TODO
    printf("Found service %s\n", serviceName);
}

void ori_setup_mdns(uint16_t port_num)
{
    DNSServiceRegister(
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

    DNSServiceErrorType err = DNSServiceProcessResult(registerRef);
    if (err != kDNSServiceErr_NoError) {
        printf("Error setting up mDNS!\n");
        exit(1);
    }

    // Find other ORI services on this network
    DNSServiceBrowse(
            &browseRef,
            0, 0,
            ORI_SERVICE_TYPE,
            NULL, // domain,
            _browse_callback,
            NULL);

    atexit(ori_deregister_mdns);
}

int ori_run_mdns(bool use_select)
{
    int fd = DNSServiceRefSockFD(browseRef);
    if (fd < 0) {
        printf("Error getting mDNS socket\n");
        return -1;
    }

    int rv = 0;
    if (use_select) {
        fd_set md_set;
        FD_SET(fd, &md_set);

        struct timeval tv_zero = {0, 0};

        rv = select(fd+1, &md_set, NULL, NULL, &tv_zero);
    }

    if (rv < 0) {
        printf("Error calling select\n");
        return -1;
    }
    else if (rv == 1 || !use_select) {
        DNSServiceErrorType err = DNSServiceProcessResult(registerRef);
        if (err != kDNSServiceErr_NoError) {
            printf("Error processing mDNS result!\n");
            exit(1);
        }
    }

    return 0;
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

std::vector<OriPeer> get_local_peers()
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

int cmd_mdnsserver(int argc, const char *argv[])
{
    printf("Starting mDNS server...\n");
    ori_setup_mdns(1020);
    while (true) {
        ori_run_mdns(true);

        std::vector<OriPeer> peers = get_local_peers();
        if (peers.size() > 0) {
            for (size_t i = 0; i < peers.size(); i++) {
                printf("Peer: %s:%d\n", peers[i].hostname.c_str(), peers[i].port);
            }
            sleep(1);
        }
    }
    return 0;
}
