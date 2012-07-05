#include <cstdlib>
#include <cstdio>
#include <arpa/inet.h>
#include <sys/select.h>

#include <dns_sd.h>

#define ORI_SERVICE_TYPE "_ori._tcp"

static DNSServiceRef sdRef;

void ori_deregister_mdns()
{
    printf("Stopping mDNS server\n");
    DNSServiceRefDeallocate(sdRef);
}

// DNSServiceRegisterReply
void _register_callback(
        DNSServiceRef sdRef,
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

    atexit(ori_deregister_mdns);
}

void ori_setup_mdns(uint16_t port_num)
{
    DNSServiceRegister(
            &sdRef,
            0, 0,
            NULL, // service name
            ORI_SERVICE_TYPE,
            NULL, // domain
            NULL, // host
            htons(port_num),
            0, NULL, // txtRecord
            _register_callback,
            NULL);

    DNSServiceErrorType err = DNSServiceProcessResult(sdRef);
    if (err != kDNSServiceErr_NoError) {
        printf("Error setting up mDNS!\n");
        exit(1);
    }

    // Find other ORI services on this network
    
}

int ori_run_mdns(bool use_select)
{
    int fd = DNSServiceRefSockFD(sdRef);
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
        DNSServiceErrorType err = DNSServiceProcessResult(sdRef);
        if (err != kDNSServiceErr_NoError) {
            printf("Error processing mDNS result!\n");
            exit(1);
        }
    }

    return 0;
}

int cmd_mdnsserver(int argc, const char *argv[])
{
    printf("Starting mDNS server...\n");
    ori_setup_mdns(1020);
    while (true) {
        //ori_run_mdns(false);
    }
    return 0;
}
