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
#include <utility>

#include "debug.h"
#include "util.h"
#include "dag.h"
#include "objecthash.h"
#include "localrepo.h"

using namespace std;

extern LocalRepo repository;

int
cmd_merge(int argc, const char *argv[])
{
    if (argc != 3) {
	cout << "merge takes two arguments!" << endl;
	cout << "usage: ori merge <commit 1> <commit 2>" << endl;
	return 1;
    }

    vector<Commit> commits = repository.listCommits();
    vector<Commit>::iterator it;

    ObjectHash p1 = ObjectHash::fromHex(argv[1]);
    ObjectHash p2 = ObjectHash::fromHex(argv[2]);

    // Find lowest common ancestor
    DAG<ObjectHash, Commit> cDag = DAG<ObjectHash, Commit>();
    ObjectHash lca;

    cDag.addNode(ObjectHash(), Commit());
    for (it = commits.begin(); it != commits.end(); it++) {
	cDag.addNode((*it).hash(), (*it));
    }

    for (it = commits.begin(); it != commits.end(); it++) {
	pair<ObjectHash, ObjectHash> p = (*it).getParents();
	cDag.addChild(p.first, (*it).hash());
	if (!p.second.isEmpty())
	    cDag.addChild(p.first, it->hash());
    }

    lca = cDag.findLCA(p1, p2);
    cout << "LCA: " << lca.hex() << endl;

    // Construct tree diffs to ancestor to find conflicts
    TreeDiff td1, td2;

    Commit c1 = repository.getCommit(p1);
    Commit c2 = repository.getCommit(p2);
    Commit cc = repository.getCommit(lca);

    Tree t1 = repository.getTree(c1.getTree());
    Tree t2 = repository.getTree(c2.getTree());
    Tree tc = repository.getTree(cc.getTree());

    td1.diffTwoTrees(t1.flattened(&repository), tc.flattened(&repository));
    td2.diffTwoTrees(t2.flattened(&repository), tc.flattened(&repository));

    printf("Tree 1:\n");
    for (size_t i = 0; i < td1.entries.size(); i++) {
        printf("%c   %s\n",
                td1.entries[i].type,
                td1.entries[i].filepath.c_str());
    }
    printf("Tree 2:\n");
    for (size_t i = 0; i < td2.entries.size(); i++) {
        printf("%c   %s\n",
                td2.entries[i].type,
                td2.entries[i].filepath.c_str());
    }

    TreeDiff mdiff;
    mdiff.mergeTrees(td1, td2);

    printf("Merged Tree:\n");
    for (size_t i = 0; i < mdiff.entries.size(); i++) {
        printf("%c   %s\n",
                mdiff.entries[i].type,
                mdiff.entries[i].filepath.c_str());
    }

    // Setup merge state
    MergeState state;
    state.setParents(p1, p2);

    repository.setMergeState(state);

    // XXX: Automatically commit if everything is resolved

    return 0;
}

