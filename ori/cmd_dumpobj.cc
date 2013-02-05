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

#include <string>
#include <iostream>

#include <ori/debug.h>
#include <ori/oriutil.h>
#include <ori/localrepo.h>

using namespace std;

extern LocalRepo repository;

int
cmd_dumpobj(int argc, const char *argv[])
{
    ObjectHash id = ObjectHash::fromHex(argv[1]);

    ObjectType type = repository.getObjectType(id);
    if (ObjectInfo::Null == type) {
        printf("Object does not exist.\n");
        return 1;
    }

    ObjectInfo info = repository.getObjectInfo(id);
    info.print();

    switch (type) {
        case ObjectInfo::Null:
        {
            NOT_IMPLEMENTED(false);
            return 1;
        }
        case ObjectInfo::Commit:
        {
            printf("\nCommit Structure:\n");
            Commit c = repository.getCommit(id);
            c.print();
            break;
        }
        case ObjectInfo::Tree:
        {
            printf("\nTree Structure:\n");
            Tree t = repository.getTree(id);
            t.print();
            break;
        }
        case ObjectInfo::Blob:
            break;
        case ObjectInfo::LargeBlob:
        {
            printf("\nChunk Table:\n");
            string rawBlob = repository.getPayload(id);
            LargeBlob lb = LargeBlob(&repository);
            lb.fromBlob(rawBlob);

            std::map<uint64_t, LBlobEntry>::iterator it;
            for (it = lb.parts.begin(); it != lb.parts.end(); it++) {
                printf("%016lx    %s %d\n", (*it).first,
                       (*it).second.hash.hex().c_str(), (*it).second.length);
            }

            break;
        }
        case ObjectInfo::Purged:
            break;
    }

    return 0;
}

