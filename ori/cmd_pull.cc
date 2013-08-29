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

#include <ori/udsclient.h>
#include <ori/udsrepo.h>
#include <ori/remoterepo.h>
#include <ori/treediff.h>

using namespace std;

extern UDSRepo repository;

int
cmd_pull(int argc, char * const argv[])
{
    string srcRoot = "";
    strwstream req;

    if (argc > 2) {
        printf("Specify a repository to pull.\n");
        printf("usage: ori pull <repo>\n");
        return 1;
    }

    if (argc == 2) {
        srcRoot = argv[1];
    }

    req.writePStr("pull");
    req.writePStr(srcRoot);

    /*
    {
        RemoteRepo::sp srcRepo(new RemoteRepo());
        if (!srcRepo->connect(srcRoot)) {
            printf("Error connecting to %s\n", srcRoot.c_str());
            return 1;
        }

        printf("Multi-pulling from %s\n", srcRoot.c_str());
        repository.multiPull(srcRepo);

        // XXX: Need to rely on sync log.
        repository.updateHead(srcRepo->get()->getHead());
    }

    // TODO: more efficient backref tracking
    printf("Rebuilding references\n");
    RefcountMap refs = repository.recomputeRefCounts();
    if (!repository.rewriteRefCounts(refs))
        return 1;
    */

    strstream resp = repository.callExt("FUSE", req.str());
    if (resp.ended()) {
        cout << "pull failed with an unknown error!" << endl;
        return 1;
    }

    switch (resp.readUInt8())
    {
        case 0:
        {
            string error;
            resp.readPStr(error);
            cout << "Pull failed: " << error << endl;
            return 0;
        }
        case 1:
        {
            ObjectHash remoteHead;
            resp.readHash(remoteHead);
            cout << "Pulled up to " << remoteHead.hex() << endl;
            return 0;
        }
        default:
            NOT_IMPLEMENTED(false);
    }

    return 0;
}

