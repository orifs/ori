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

#include <stdint.h>

#include <sys/types.h>

#include <string>
#include <tr1/memory>

#include "repo.h"
#include "localrepo.h"
#include "sshrepo.h"
#include "sshclient.h"
#include "httprepo.h"
#include "httpclient.h"
#include "remoterepo.h"
#include "util.h"

using namespace std;

RemoteRepo::RemoteRepo()
    : r(NULL)
{
}

RemoteRepo::~RemoteRepo()
{
    disconnect();
}

bool
RemoteRepo::connect(const string &url)
{
    this->url = url;
    if (Util_IsPathRemote(url)) {
        if (strncmp(url.c_str(), "http://", 7) == 0) {
            hc.reset(new HttpClient(url));
            r = new HttpRepo(hc.get());
            return (hc->connect() == 0);
        } else {
            sc.reset(new SshClient(url));
            r = new SshRepo(sc.get());
            return (sc->connect() == 0);
        }
    } else {
	LocalRepo *lr = new LocalRepo(url);
	r = lr;
        return lr->open(url);
    }

    return false;
}

void
RemoteRepo::disconnect()
{
    if (hc)
	hc->disconnect();
    if (sc)
	sc->disconnect();
    if (r)
	delete r;
}

Repo *
RemoteRepo::operator->()
{
    return r;
}

Repo *
RemoteRepo::get()
{
    return r;
}

