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

#include <stdint.h>
#include <stdio.h>

#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string>
#include <vector>
#include <map>
#include <iostream>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#include <event2/util.h>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/oricrypt.h>
#include <oriutil/orinet.h>
#include <oriutil/systemexception.h>
#include <oriutil/thread.h>
#include <oriutil/kvserializer.h>
#include <ori/localrepo.h>

#include "orisyncconf.h"
#include "repoinfo.h"
#include "hostinfo.h"

using namespace std;

// Announcement UDP port
#define ORISYNC_UDPPORT         8051
// Advertisement interval
#define ORISYNC_ADVINTERVAL     5

OriSyncConf rc;

HostInfo myInfo;
RWLock infoLock;
map<string, HostInfo> hosts;
RWLock hostsLock;

class Announcer : public Thread
{
public:
    Announcer() : Thread() {
        int status;
        int broadcast = 1;

        fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (fd < 0) {
            throw SystemException();
        }

        status = setsockopt(fd, SOL_SOCKET, SO_BROADCAST,
                            &broadcast, sizeof(broadcast));
        if (status < 0) {
            close(fd);
            throw SystemException();
        }

        memset(&dstAddr, 0, sizeof(dstAddr));
        dstAddr.sin_family = AF_INET;
        dstAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
        dstAddr.sin_port = htons(ORISYNC_UDPPORT);
    }
    ~Announcer() {
        close(fd);
    }
    string generate() {
        ReaderKey key(&infoLock);
        char buf[32];
        string msg;

        // First 31 bytes of cluster with null
        memset(buf, 0, 32);
        strncpy(buf, rc.getCluster().c_str(), 31);
        msg.assign(buf, 32);
        msg.append(OriCrypt_Encrypt(myInfo.getBlob(), rc.getKey()));

        return msg;
    }
    void run() {
        while (1) {
            int status;
            string msg;
            size_t len;

            msg = generate();
            len = msg.size();

            status = sendto(fd, msg.c_str(), len, 0,
                            (struct sockaddr *)&dstAddr,
                            sizeof(dstAddr));
            if (status < 0) {
                perror("sendto");
            }

            sleep(ORISYNC_ADVINTERVAL);
        }
    }
private:
    int fd;
    struct sockaddr_in dstAddr;
};

class Listener : public Thread
{
public:
    Listener() : Thread() {
        int status;
        struct sockaddr_in addr;
        int reuseaddr = 1;

        fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (fd < 0) {
            throw SystemException();
        }

        status = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                            &reuseaddr, sizeof(reuseaddr));
        if (status < 0) {
            perror("setsockopt");
            close(fd);
            throw SystemException();
        }
#ifdef __APPLE__
        status = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT,
                            &reuseaddr, sizeof(reuseaddr));
        if (status < 0) {
            perror("setsockopt");
            close(fd);
            throw SystemException();
        }
#endif /* __APPLE__ */

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(ORISYNC_UDPPORT);

        status = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
        if (status < 0) {
            perror("bind");
            close(fd);
            throw SystemException();
        }
    }
    ~Listener() {
        close(fd);
    }
    void updateHost(KVSerializer &kv) {
        WriterKey key(&hostsLock);
        map<string, HostInfo>::iterator it;
        string hostId = kv.getStr("hostId");

        // Update
        it = hosts.find(hostId);
        if (it == hosts.end()) {
            hosts[hostId] = HostInfo(hostId, kv.getStr("cluster"));
        }
        hosts[hostId].update(kv);
    }
    void parse(const char *buf, int len) {
        string ctxt;
        string ptxt;
        KVSerializer kv;

        // Message too small
        if (len < 32)
            return;

        // Cluster ID
        ctxt.assign(buf, 32);
        ctxt[31] = '\0';
        if (strncmp(ctxt.c_str(), rc.getCluster().c_str(), 31) != 0)
            return;

        ctxt.assign(buf+32, len-32);
        ptxt = OriCrypt_Decrypt(ctxt, rc.getKey());
        Util_PrintHex(ptxt);
        try {
            kv.fromBlob(ptxt);

            // Ignore requests from self
            if (kv.getStr("hostId") == rc.getUUID())
                return;

            // Ignore messages from other clusters
            if (kv.getStr("cluster") != rc.getCluster())
                return;

            // Add or update hostinfo
            updateHost(kv);
        } catch(SerializationException e) {
            LOG("Error encountered parsing announcement: %s", e.what());
            return;
        }
    }
    void dumpHosts() {
        ReaderKey(hostsLock);
        map<string, HostInfo>::iterator it;

        cout << "=== Begin Hosts ===" << endl;
        for (it = hosts.begin(); it != hosts.end(); it++) {
            cout << it->second.getHost() << endl;
        }
        cout << "==== End Hosts ====" << endl;
    }
    void run() {
        while (1) {
            char buf[1500];
            int len = 1500;
            struct sockaddr_in srcAddr;
            socklen_t srcAddrLen = sizeof(srcAddr);

            len = recvfrom(fd, buf, len, 0,
                           (struct sockaddr *)&srcAddr,
                           &srcAddrLen);
            if (len < 0) {
                perror("recvfrom");
                continue;
            }
            parse(buf, len);

            dumpHosts();
        }
    }
private:
    int fd;
};

Announcer *announcer;
Listener *listener;

void
Httpd_getRoot(struct evhttp_request *req, void *arg)
{
    struct evbuffer *buf;

    buf = evbuffer_new();
    if (buf == NULL) {
        evhttp_send_error(req, HTTP_INTERNAL, "Internal Error");
        return;
    }

    evbuffer_add_printf(buf, "Temporary String");
    evhttp_add_header(req->output_headers, "Content-Type", "text/html");
    evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

int
start_server()
{
    rc = OriSyncConf();
    announcer = new Announcer();
    listener = new Listener();

    myInfo = HostInfo(rc.getUUID(), rc.getCluster());
    // XXX: Update addresses periodically
    myInfo.setHost(Util_StringJoin(OriNet_GetAddrs(), ','));

    announcer->start();
    listener->start();

    struct event_base *base = event_base_new();
    struct evhttp *httpd = evhttp_new(base);
    evhttp_bind_socket(httpd, "0.0.0.0", 8051);

    evhttp_set_cb(httpd, "/", Httpd_getRoot, NULL);

    event_base_dispatch(base);
    evhttp_free(httpd);

    return 0;
}

