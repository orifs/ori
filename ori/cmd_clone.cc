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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>
#include <iostream>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <ori/localrepo.h>
#include <ori/remoterepo.h>
#include <ori/treediff.h>

using namespace std;

extern LocalRepo repository;

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
	RemoteRepo srcRepo;
	srcRepo.connect(srcRoot);

        // XXX: Need to rely on sync log.
        ObjectHash head = srcRepo->getHead();

        dstRepo.pull(srcRepo.get());

        if (!head.isEmpty())
            dstRepo.updateHead(head);
    }

    // TODO: rebuild references
    RefcountMap refs = dstRepo.recomputeRefCounts();
    if (!dstRepo.rewriteRefCounts(refs))
        return 1;

    return 0;
}

