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

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <ori/localrepo.h>

#include "fuse_cmd.h"

using namespace std;

extern LocalRepo repository;

void
usage_commit(void)
{
    cout << "ori commit [MESSAGE]" << endl;
    cout << endl;
    cout << "Commit any outstanding changes into the repository." << endl;
    cout << endl;
    cout << "An optional message can be added to the commit." << endl;
}

int
cmd_commit(int argc, const char *argv[])
{
    if (OF_RunCommand("commit"))
        return 0;

    Commit c;
    Tree tip_tree;
    ObjectHash tip = repository.getHead();
    if (tip != EMPTY_COMMIT) {
        c = repository.getCommit(tip);
        tip_tree = repository.getTree(c.getTree());
    }

    TreeDiff diff;
    diff.diffToDir(c, repository.getRootPath(), &repository);
    if (diff.entries.size() == 0) {
        cout << "Nothing to commit!" << endl;
        return 0;
    }

    Tree new_tree = diff.applyTo(tip_tree.flattened(&repository),
            &repository);

    Commit newCommit;
    if (argc == 2) {
        newCommit.setMessage(argv[1]);
    }
    repository.commitFromTree(new_tree.hash(), newCommit);

    return 0;
}

