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
#include <boost/tr1/memory.hpp>

#include <oriutil/oriutil.h>
#include <oriutil/systemexception.h>
#include <ori/repo.h>
#include <ori/localrepo.h>
#include <ori/sshrepo.h>
#include <ori/sshclient.h>
#include <ori/httprepo.h>
#include <ori/httpclient.h>
#include <ori/udsrepo.h>
#include <ori/udsclient.h>
#include <ori/remoterepo.h>

using namespace std;
#ifdef HAVE_CXXTR1
using namespace std::tr1;
#endif /* HAVE_CXXTR1 */

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
        UDSClient *udsClient;
        try {
            udsClient = new UDSClient(url);
            int status = udsClient->connect();
            uc.reset(udsClient);
            r = new UDSRepo(uc.get());
            return (status == 0);
        } catch (SystemException e) {
            delete udsClient;
        }
        LocalRepo *lr = new LocalRepo(url);
        r = lr;
        lr->open(url);
        return true;
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
    if (uc)
        uc->disconnect();
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

