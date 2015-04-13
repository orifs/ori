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

#ifndef __HTTPSERVER_H__
#define __HTTPSERVER_H__

#include <event2/event.h>
#include <event2/http.h>
#include <event2/http_struct.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

class HTTPServer
{
public:
    HTTPServer(LocalRepo &repository, uint16_t port);
    ~HTTPServer();
    void start(bool mDNSEnable);
    void stop();
protected:
    void entry(struct evhttp_request *req);
private:
    int authenticate(struct evhttp_request *req, struct evbuffer *buf);
    // Handlers
    void stop(struct evhttp_request *req);
    void getId(struct evhttp_request *req);
    void getVersion(struct evhttp_request *req);
    void head(struct evhttp_request *req);
    void getIndex(struct evhttp_request *req);
    void getCommits(struct evhttp_request *req);
    void contains(struct evhttp_request *req);
    void getObjs(struct evhttp_request *req);
    void getObjInfo(struct evhttp_request *req);
    LocalRepo &repo;
    struct evhttp *httpd;
    /* set if a test needs to call loopexit on a base */
    struct event_base *base;
    friend void HTTPServerReqHandlerCB(struct evhttp_request *req, void *arg);
};

#endif

