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

#define _WITH_DPRINTF

#include <cstdlib>
#include <cstdio>
#include <cassert>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "httpclient.h"
#include "httprepo.h"
#include "util.h"

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
        assert(false);
    }

    portPos = tmp.find(':');
    pathPos = tmp.find('/');
    if (portPos != -1) {
        assert(portPos < pathPos);
        remotePort = tmp.substr(portPos + 1, pathPos - portPos);
    } else {
        portPos = pathPos;
    }
    assert(pathPos != -1);

    remoteHost = tmp.substr(0, portPos);
    remoteRepo = tmp.substr(pathPos + 1);
}

HttpClient::~HttpClient()
{
    // Close
}

int HttpClient::connect()
{
    // Wait for READY from server
    std::string ready;
    recvResponse(ready);
    if (ready != "READY\n") {
        return -1;
    }

    return 0;
}

bool HttpClient::connected() {
    return false;
}

void HttpClient::sendCommand(const std::string &command) {
}

void HttpClient::recvResponse(std::string &out) {
}


int cmd_httpclient(int argc, const char *argv[]) {
    if (argc < 2) {
        printf("Usage: ori httpclient http://hostname:port/path\n");
        exit(1);
    }

    HttpClient client = HttpClient(std::string(argv[1]));
    if (client.connect() < 0) {
        printf("Error connecting to %s\n", argv[1]);
        exit(1);
    }

    dprintf(STDOUT_FILENO, "Connected\n");

    HttpRepo repo(&client);
    std::set<ObjectInfo> objs = repo.listObjects();
    for (std::set<ObjectInfo>::iterator it = objs.begin();
            it != objs.end();
            it++) {
        printf("%s\n", (*it).hash.c_str());
    }

    printf("Terminating HTTP connection...\n");
    return 0;
}
