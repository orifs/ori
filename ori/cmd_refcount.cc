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

#include <stdbool.h>
#include <stdint.h>

#include <string>
#include <iostream>
#include <iomanip>

#include "debug.h"
#include "util.h"
#include "localrepo.h"

using namespace std;

extern LocalRepo repository;

/*
 * Print the reference counts for all objects, or show the references
 * to a particular object.
 */
int
cmd_refcount(int argc, const char *argv[])
{
    const MetadataLog &md = repository.getMetadata();
    if (argc == 1) {
        cout << left << setw(64) << "Object" << " Count" << endl;

        set<ObjectInfo> objs = repository.listObjects();
        for (set<ObjectInfo>::iterator it = objs.begin();
                it != objs.end();
                it++) {
            cout << (*it).hash.hex() << " " << md.getRefCount((*it).hash) << endl;
        }
    } else if (argc == 2) {
        ObjectHash hash = ObjectHash::fromHex(argv[1]);
        cout << hash.hex() << " " << md.getRefCount(hash) << endl;
    } else {
        cout << "Invalid number of arguements." << endl;
        cout << "ori refcount [OBJID]" << endl;
    }

    return 0;
}

