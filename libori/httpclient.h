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

#ifndef __HTTPCLIENT_H__
#define __HTTPCLIENT_H__

#include <string>

void HttpClient_requestDoneCB(struct evhttp_request *, void *);

class HttpClient
{
public:
    HttpClient(const std::string &remotePath);
    ~HttpClient();

    int connect();
    void disconnect();
    bool connected();

    // At the moment the protocol is synchronous
    int getRequest(const std::string &command,
                   std::string &response);
    int postRequest(const std::string &url,
                    const std::string &payload,
                    std::string &response);
    int putRequest(const std::string &command,
                   const std::string &payload,
                   std::string &response);

private:
    struct event_base *base;
    struct evdns_base *dnsBase;
    struct evhttp_connection *con;
    std::string remoteHost, remotePort, remoteRepo;
    friend void HttpClient_requestDoneCB(struct evhttp_request *,
                                         void *);
};

#endif /* __HTTPCLIENT_H__ */
