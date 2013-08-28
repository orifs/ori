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
#include <iomanip>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <ori/localrepo.h>

using namespace std;

extern LocalRepo repository;

extern "C" {
#include <libdiffmerge/blob.h>
int *text_diff(Blob *pA_Blob, Blob *pB_Blob, Blob *pOut, uint64_t diffFlags);
};

int
cmd_diff(int argc, char * const argv[])
{
    Commit c;
    ObjectHash tip = repository.getHead();
    if (tip != EMPTY_COMMIT) {
        c = repository.getCommit(tip);
    }

    TreeDiff td;
    td.diffToDir(c, repository.getRootPath(), &repository);

    Blob a, b, out;

    for (size_t i = 0; i < td.entries.size(); i++) {
        if (td.entries[i].type == TreeDiffEntry::Modified) {
            printf("Index: %s\n", td.entries[i].filepath.c_str());
            printf("==================================================================\n");
            // XXX: Handle file move
            printf("--- %s\n+++ %s\n",
                   td.entries[i].filepath.c_str(),
                   td.entries[i].filepath.c_str());

            // print diff
            string path = repository.getRootPath() + td.entries[i].filepath;
            string buf = repository.getPayload(td.entries[i].hashes.first);

            blob_init(&a, buf.data(), buf.size());
            blob_read_from_file(&b, path.c_str());
            blob_zero(&out);
            text_diff(&a, &b, &out, 0);
            blob_write_to_file(&out, "-");
        }
    }

    return 0;
}

