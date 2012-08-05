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
    if (Util_IsPathRemote(url)) {
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
cmd_clone(int argc, const char *argv[])
{
    int status;
    string srcRoot;
    string newRoot;

    if (argc != 2 && argc != 3) {
	printf("Specify a repository to clone.\n");
	printf("usage: ori clone <repo> [<dir>]\n");
	return 1;
    }

    srcRoot = argv[1];
    if (argc == 2) {
	newRoot = srcRoot.substr(srcRoot.rfind("/")+1);
    } else {
	newRoot = argv[2];
    }
    if (!Util_FileExists(newRoot)) {
        mkdir(newRoot.c_str(), 0755);
    }
    status = LocalRepo_Init(newRoot);
    if (status != 0) {
        printf("Failed to construct an empty repository!\n");
        return 1;
    }

    printf("Cloning from %s to %s\n", srcRoot.c_str(), newRoot.c_str());

    LocalRepo dstRepo;
    dstRepo.open(newRoot);

    // Setup remote pointer
    string originPath = srcRoot;
    if (!Util_IsPathRemote(srcRoot)) {
	originPath = Util_RealPath(srcRoot);
    }
    dstRepo.addPeer("origin", originPath);

    {
        std::tr1::shared_ptr<Repo> srcRepo;
        std::tr1::shared_ptr<HttpClient> httpClient;
        std::tr1::shared_ptr<SshClient> sshClient;
        getRepoFromURL(srcRoot, srcRepo, httpClient, sshClient);

        dstRepo.pull(srcRepo.get());

        // XXX: Need to rely on sync log.
        dstRepo.updateHead(srcRepo->getHead());
    }

    // TODO: rebuild references
    RefcountMap refs = dstRepo.recomputeRefCounts();
    if (!dstRepo.rewriteRefCounts(refs))
        return 1;

    return 0;
}

