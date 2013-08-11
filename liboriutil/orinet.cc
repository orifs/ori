/*
 * Copyright (c) 2013 Stanford University
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

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>

#include <string>
#include <list>
#include <exception>
#include <stdexcept>

#include <oriutil/debug.h>
#include <oriutil/systemexception.h>
#include <oriutil/oriutil.h>
#include <oriutil/orinet.h>

using namespace std;

std::string
OriNet_ResolveHost(const string &hostname) {
    struct addrinfo hints;
    struct addrinfo *result;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    //hints.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED;

    int status = getaddrinfo(hostname.c_str(), "80", &hints, &result);
    if (status < 0 || result == NULL) {
        perror("getaddrinfo");
        return "";
    }

    struct sockaddr_in *sa = (struct sockaddr_in *)result->ai_addr;
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sa->sin_addr, buf, INET_ADDRSTRLEN);

    freeaddrinfo(result);

    DLOG("Resolved IP addr: %s", buf);
    return buf;
}

vector<string>
OriNet_GetAddrs()
{
    struct ifaddrs *ifaddr, *ifa;
    vector<string> rval;

    if (getifaddrs(&ifaddr) == -1) {
        throw SystemException();
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        int family;
        int status;
        char hostAddr[NI_MAXHOST];

        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;
        if (family != AF_INET)
            continue;

        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        if (addr->sin_addr.s_addr == htonl(INADDR_LOOPBACK))
            continue;

        status = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                             hostAddr, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
        if (status != 0) {
            freeifaddrs(ifaddr);
            throw SystemException(EINVAL);
        }

        rval.push_back(hostAddr);
    }

    freeifaddrs(ifaddr);

    return rval;
}

