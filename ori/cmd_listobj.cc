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

#include <set>
#include <string>
#include <iostream>

#include <oriutil/debug.h>
#include <ori/localrepo.h>

using namespace std;

extern LocalRepo repository;

int
cmd_listobj(int argc, char * const argv[])
{
    set<ObjectInfo> objects = repository.listObjects();
    set<ObjectInfo>::iterator it;

    for (it = objects.begin(); it != objects.end(); it++)
    {
        const char *type;
        switch ((*it).type)
        {
            case ObjectInfo::Commit:
                type = "Commit";
                break;
            case ObjectInfo::Tree:
                type = "Tree";
                break;
            case ObjectInfo::Blob:
                type = "Blob";
                break;
            case ObjectInfo::LargeBlob:
                type = "LargeBlob";
                break;
            case ObjectInfo::Purged:
                type = "Purged";
                break;
            default:
                cout << "Unknown object type (id " << (*it).hash.hex() << ")!" << endl;
                PANIC();
        }
        cout << (*it).hash.hex() << " # " << type << endl;
    }

    return 0;
}

