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

#include <string>
#include <iostream>

#include <ori/debug.h>
#include <ori/localrepo.h>

using namespace std;

extern LocalRepo repository;

/*
 * Find lost Heads
 */
int
cmd_findheads(int argc, const char *argv[])
{
    RefcountMap refs = repository.recomputeRefCounts();

    for (RefcountMap::iterator it = refs.begin();
            it != refs.end();
            it++) {
        if ((*it).first == EMPTY_COMMIT)
            continue;

        if ((*it).second == 0
            && repository.getObjectType((*it).first)) {
            Commit c = repository.getCommit((*it).first);

            // XXX: Check for existing branch names

            cout << "commit:  " << (*it).first.hex() << endl;
            cout << "parents: " << c.getParents().first.hex() << endl;
            cout << c.getMessage() << endl;
        }
    }

    return 0;
}

