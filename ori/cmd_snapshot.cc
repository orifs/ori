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

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <iostream>

#include "debug.h"
#include "scan.h"
#include "util.h"
#include "localrepo.h"

using namespace std;

extern LocalRepo repository;

/*int
commitHelper(void *arg, const char *path)
{
    string filePath = path;
    Tree *tree = (Tree *)arg;

    if (Util_IsDirectory(path)) {
        string hash;
	Tree subTree = Tree();

	Scan_Traverse(path, &subTree, commitHelper);

	hash = repository.addTree(subTree);
        tree->addObject(filePath.c_str(), hash);
    } else {
        pair<string, string> pHash = repository.addFile(path);
        tree->addObject(filePath.c_str(), pHash.first, pHash.second);
    }

    return 0;
}

int
cmd_oldcommit(int argc, const char *argv[])
{
    string blob;
    string treeHash, commitHash;
    string msg;
    string user;
    Tree tree = Tree();
    Commit commit = Commit();
    string root = LocalRepo::findRootPath();

    if (argc == 1) {
        msg = "No message.";
    } else if (argc == 2) {
        msg = argv[1];
    }

    Scan_Traverse(root.c_str(), &tree, commitHelper);

    treeHash = repository.addTree(tree);

    // XXX: Get parents
    commit.setTree(treeHash);
    commit.setParents(repository.getHead());
    commit.setMessage(msg);
    commit.setTime(time(NULL));

    user = Util_GetFullname();
    if (user != "")
        commit.setUser(user);

    commitHash = repository.addCommit(commit);

    // Update .ori/HEAD
    repository.updateHead(commitHash);

    printf("Commit Hash: %s\nTree Hash: %s\n%s",
	   commitHash.c_str(),
	   treeHash.c_str(),
	   blob.c_str());

    return 0;
}*/

int
cmd_snapshot(int argc, const char *argv[])
{
    /*if (OF_RunCommand("snapshot"))
        return 0;*/

    if (argc != 2) {
	cout << "Specify a snapshot name." << endl;
	cout << "usage: ori snapshot <snapshot name>" << endl;
	return 1;
    }

    string snapshotName = argv[1];

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
    newCommit.setMessage("Created snapshot."); // TODO: allow user to specify message
    newCommit.setSnapshot(snapshotName);
    repository.commitFromTree(new_tree.hash(), newCommit);



    /*string blob;
    string treeHash, commitHash;
    string snapshotName;
    string msg;
    string user;
    Tree tree = Tree();
    Commit commit = Commit();
    string root = LocalRepo::findRootPath();

    snapshotName = argv[1];
    msg = "Created snapshot."; // XXX: Allow users to specify a snapshot

    Scan_Traverse(root.c_str(), &tree, commitHelper);

    treeHash = repository.addTree(tree);

    // XXX: Get parents
    commit.setTree(treeHash);
    commit.setParents(repository.getHead());
    commit.setMessage(msg);
    commit.setTime(time(NULL));
    commit.setSnapshot(snapshotName);

    user = Util_GetFullname();
    if (user != "")
        commit.setUser(user);

    commitHash = repository.addCommit(commit);

    // Update .ori/HEAD
    repository.updateHead(commitHash);

    printf("Commit Hash: %s\nTree Hash: %s\n%s",
	   commitHash.c_str(),
	   treeHash.c_str(),
	   blob.c_str());*/

    return 0;
}

