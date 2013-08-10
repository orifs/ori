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

using namespace std;

extern LocalRepo repository;

int
cmd_treediff(int argc, char * const argv[])
{
    if (argc != 3) {
	cout << "treediff takes two arguments!" << endl;
	cout << "usage: ori treediff <commit 1> <commit 2>" << endl;
	return 1;
    }
    TreeDiff td;

    Commit c1 = repository.getCommit(ObjectHash::fromHex(argv[1]));
    Commit c2 = repository.getCommit(ObjectHash::fromHex(argv[2]));

    Tree t1 = repository.getTree(c1.getTree());
    Tree t2 = repository.getTree(c2.getTree());

    td.diffTwoTrees(t1.flattened(&repository), t2.flattened(&repository));

    for (size_t i = 0; i < td.entries.size(); i++) {
        printf("%c   %s\n",
                td.entries[i].type,
                td.entries[i].filepath.c_str());
    }

    return 0;
}

