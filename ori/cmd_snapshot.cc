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

#include <getopt.h>

#include <string>
#include <iostream>

#include <ori/localrepo.h>

#include "fuse_cmd.h"

using namespace std;

extern LocalRepo repository;

void
usage_snapshot(void)
{
    cout << "ori snapshot [OPTIONS] [SNAPSHOT NAME]" << endl;
    cout << endl;
    cout << "Create a snapshot of the file system." << endl;
    cout << endl;
    cout << "An additional snapshot name may be set, but it may only" << endl;
    cout << "contain letters, numbers, underscores, and periods." << endl;
    cout << endl;
    cout << "Options:" << endl;
    cout << "    -m message     Add a message to the snapshot" << endl;
}

int
cmd_snapshot(int argc, char * const argv[])
{
    int ch;
    bool hasMsg = false;
    bool hasName = false;
    string msg;
    string name;

    if (OF_RunCommand("snapshot"))
        return 0;

    struct option longopts[] = {
        { "message",    required_argument,  NULL,   'm' },
        { NULL,         0,                  NULL,   0   }
    };

    while ((ch = getopt_long(argc, argv, "mh", longopts, NULL)) != -1) {
        switch (ch) {
            case 'm':
                hasMsg = true;
                msg = optarg;
                break;
            default:
                printf("Usage: ori snapshot [OPTIONS] [SNAPSHOT NAME]\n");
                return 1;
        }
    }
    argc -= optind;
    argv += optind;

    if (argc != 0 && argc != 1) {
        cout << "Wrong number of arguments." << endl;
        cout << "Usage: ori snapshot [OPTIONS] [SNAPSHOT NAME]" << endl;
        return 1;
    }

    if (argc == 1) {
        hasName = true;
        name = argv[0];
    }

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
        cout << "Note: nothing to commit" << endl;
    }

    Tree new_tree = diff.applyTo(tip_tree.flattened(&repository),
            &repository);

    Commit newCommit;

    if (hasMsg)
        newCommit.setMessage(msg);
    if (hasName) {
        newCommit.setSnapshot(name);
        if (!hasMsg)
            newCommit.setMessage("Created snapshot '" + name + "'");
    }

    repository.commitFromTree(new_tree.hash(), newCommit);

    return 0;
}

