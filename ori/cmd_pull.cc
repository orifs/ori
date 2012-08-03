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

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>
#include <iostream>
#include <tr1/memory>

#include "debug.h"
#include "scan.h"
#include "util.h"
#include "localrepo.h"
#include "httpclient.h"
#include "httprepo.h"
#include "sshclient.h"
#include "sshrepo.h"
#include "treediff.h"

using namespace std;

extern LocalRepo repository;

static int
getRepoFromURL(const string &url,
        std::tr1::shared_ptr<Repo> &r,
        std::tr1::shared_ptr<HttpClient> &hc,
        std::tr1::shared_ptr<SshClient> &sc)
{
    if (Util_IsPathRemote(url.c_str())) {
        if (strncmp(url.c_str(), "http://", 7) == 0) {
            hc.reset(new HttpClient(url));
            r.reset(new HttpRepo(hc.get()));
            hc->connect();
        } else {
            sc.reset(new SshClient(url));
            r.reset(new SshRepo(sc.get()));
            sc->connect();
        }
    } else {
        r.reset(new LocalRepo(url));
        ((LocalRepo *)r.get())->open(url);
    }

    return 0; // TODO: errors?
}

int
cmd_pull(int argc, const char *argv[])
{
    string srcRoot;

    if (argc != 2) {
	printf("Specify a repository to pull.\n");
	printf("usage: ori pull <repo>\n");
	return 1;
    }

    srcRoot = argv[1];

    {
        std::tr1::shared_ptr<Repo> srcRepo;
        std::tr1::shared_ptr<HttpClient> httpClient;
        std::tr1::shared_ptr<SshClient> sshClient;
        getRepoFromURL(srcRoot, srcRepo, httpClient, sshClient);

        printf("Pulling from %s\n", srcRoot.c_str());
        repository.pull(srcRepo.get());

        // XXX: Need to rely on sync log.
        repository.updateHead(srcRepo->getHead());
    }

    // TODO: more efficient backref tracking
    printf("Rebuilding references\n");
    RefcountMap refs = repository.recomputeRefCounts();
    if (!repository.rewriteRefCounts(refs))
        return 1;

    return 0;
}

