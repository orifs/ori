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

#ifndef __REMOTEREPO_H__
#define __REMOTEREPO_H__

#include <string>
#include <memory>

#include "repo.h"
#include "httpclient.h"
#include "sshclient.h"
#include "udsclient.h"

class RemoteRepo
{
public:
    typedef std::shared_ptr<RemoteRepo> sp;

    RemoteRepo();
    ~RemoteRepo();
    bool connect(const std::string &url);
    void disconnect();
    Repo *operator->();
    Repo *get();

    const std::string &getURL() const {
        return url;
    }
private:
    Repo *r;
    std::shared_ptr<HttpClient> hc;
    std::shared_ptr<SshClient> sc;
    std::shared_ptr<UDSClient> uc;
    std::string url;
};

#endif /* __REMOTEREPO_H__ */

