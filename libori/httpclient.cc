/*
 * Copyright (c) 2012-2013 Stanford University
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

#include <cstdlib>
#include <cstdio>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <event2/event.h>
#include <event2/dns.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#include <string>
#include <iostream>
#include <vector>

#include <oriutil/debug.h>
#include <oriutil/orinet.h>
#include <oriutil/oriutil.h>
#include <ori/httpclient.h>
#include <ori/httprepo.h>

#define D_READ 0
#define D_WRITE 1

using namespace std;

/*
 * HttpClient
 */
HttpClient::HttpClient(const std::string &remotePath)
{
    string tmp;
    size_t portPos, pathPos;

    if (remotePath.substr(0, 7) == "http://") {
        tmp = remotePath.substr(7);
    } else {
        ASSERT(false);
    }

    portPos = tmp.find(':');
    pathPos = tmp.find('/');
    if (portPos != string::npos) {
        ASSERT(portPos < pathPos);
        remotePort = tmp.substr(portPos + 1, pathPos - portPos - 1);
    } else {
        portPos = pathPos;
        remotePort = "80";
    }
    ASSERT(pathPos != string::npos);

    remoteHost = tmp.substr(0, portPos);
    remoteRepo = tmp.substr(pathPos + 1);

    DLOG("libevent %s", event_get_version());
}

HttpClient::~HttpClient()
{
    // Close
}

int
HttpClient::connect()
{
    string id;
    string version;
    string headRev;
    uint16_t port;

    base = event_base_new();
    dnsBase = evdns_base_new(base, /* Add DNS servers */ 0);
    if (dnsBase == NULL) {
        WARNING("HTTP client couldn't set up evdns!");
        return -1;
    }

    port = strtoul(remotePort.c_str(), NULL, 10);

    // Can't get evdns to work, using Util_ResolveHost
    std::string remoteIP = OriNet_ResolveHost(remoteHost);
    con = evhttp_connection_base_new(base, dnsBase, remoteIP.c_str(), port);
    if (con == NULL) {
        WARNING("HTTP client couldn't set up connection!");
        return -1;
    }

    return 0;
}


void
HttpClient::disconnect()
{
    if (con)
        evhttp_connection_free(con);
    if (dnsBase)
        evdns_base_free(dnsBase, 0);
    if (base)
        event_base_free(base);
    con = NULL;
    dnsBase = NULL;
    base = NULL;
}

bool
HttpClient::connected()
{
    return false;
}

struct RequestCB
{
    HttpClient *client;
    string *response;
};

void
HttpClient_requestDoneCB(struct evhttp_request *req, void *r)
{
    int status;
    RequestCB *cb = (RequestCB *)r;
    HttpClient *client = cb->client;
    //struct evkeyvalq *headers;
    struct evbuffer *bufIn;

    if (!req) {
        WARNING("req is NULL!");
        event_base_loopexit(client->base, NULL);
        return;
    }

    status = evhttp_request_get_response_code(req);
    if (status != HTTP_OK) {
        WARNING("HTTP request failed!");
        event_base_loopexit(client->base, NULL);
        return;
    }

    //headers = evhttp_request_get_input_headers(req);
    bufIn = evhttp_request_get_input_buffer(req);

    /*
     * XXX: This is a hack to prevent the corruption we were seeing. Not sure 
     * why this works, but the data needs to be read out here into a string and 
     * we should signal the client to wakeup or register a callback.
     */
    int len = evbuffer_get_length(bufIn);
    char *data = (char *)evbuffer_pullup(bufIn, len);
    if (data == NULL) {
        WARNING("Error running evbuffer_pullup");
        event_base_loopexit(client->base, NULL);
        return;
    }

    cb->response->assign(data, len);

    event_base_loopexit(client->base, NULL);
}

int
HttpClient::getRequest(const string &command, string &response)
{
    int status;
    RequestCB cb;
    struct evhttp_request *req;

    cb.client = this;
    cb.response = &response;
    req = evhttp_request_new(HttpClient_requestDoneCB, (void *)&cb);

    struct evkeyvalq *headers = evhttp_request_get_output_headers(req);
    evhttp_add_header(headers, "Connection", "keep-alive");

    status = evhttp_make_request(con, req, EVHTTP_REQ_GET, command.c_str());
    if (status < 0) {
        WARNING("HTTP request failure!");
        return -1;
    }

    // XXX: Create dedicated event loop
    event_base_dispatch(base);

    return 0;
}

int
HttpClient::postRequest(const string &url,
                        const string &payload,
                        string &response)
{
    RequestCB cb;
    cb.client = this;
    cb.response = &response;

    struct evhttp_request *req = evhttp_request_new(
            HttpClient_requestDoneCB, &cb);

    struct evkeyvalq *headers = evhttp_request_get_output_headers(req);
    evhttp_add_header(headers, "Connection", "keep-alive");

    struct evbuffer *outbuf = evhttp_request_get_output_buffer(req);
    evbuffer_add(outbuf, payload.data(), payload.size());

    int status = evhttp_make_request(con, req, EVHTTP_REQ_POST, url.c_str());
    if (status < 0) {
        WARNING("HTTP request failure!");
        return -1;
    }

    // XXX: Create dedicated event loop
    event_base_dispatch(base);

    return 0;
}

int
HttpClient::putRequest(const string &command,
                       const string &payload,
                       string &response)
{
    NOT_IMPLEMENTED(false);
    return -1;
}

int
cmd_httpclient(int argc, const char *argv[])
{
    if (argc < 2) {
        printf("Usage: ori httpclient http://hostname:port/path\n");
        exit(1);
    }

    HttpClient client = HttpClient(std::string(argv[1]));
    if (client.connect() < 0) {
        printf("Error connecting to %s\n", argv[1]);
        exit(1);
    }

    cout << "Connected\n" << endl;

    string repoId;
    string ver;
    string headId;
    client.getRequest("/id", repoId);
    client.getRequest("/version", ver);
    client.getRequest("/HEAD", headId);

    cout << "Repository ID: " << repoId << endl;
    cout << "Version: " << ver << endl;
    cout << "HEAD: " << headId << endl << endl;

    HttpRepo repo(&client);
    set<ObjectInfo> objs = repo.listObjects();
    for (set<ObjectInfo>::iterator it = objs.begin();
         it != objs.end();
         it++) {
        printf("%s\n", (*it).hash.hex().c_str());
    }

    printf("Terminating HTTP connection...\n");
    return 0;
}
