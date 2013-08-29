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

#include <ori/localrepo.h>

#include "fuse_cmd.h"

using namespace std;

extern LocalRepo repository;

int
cmd_status(int argc, char * const argv[])
{
    return 1;
    //if (OF_RunCommand("status"))
    //    return 0;

    Commit c;
    ObjectHash tip = repository.getHead();
    if (tip != EMPTY_COMMIT) {
        c = repository.getCommit(tip);
    }

    TreeDiff td;
    td.diffToDir(c, repository.getRootPath(), &repository);

    for (size_t i = 0; i < td.entries.size(); i++) {
        printf("%c   %s\n",
                td.entries[i].type,
                td.entries[i].filepath.c_str());
    }

    return 0;
}

