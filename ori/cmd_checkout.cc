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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>
#include <iostream>
#include <iomanip>

#include <ori/debug.h>
#include <ori/scan.h>
#include <ori/oriutil.h>
#include <ori/localrepo.h>

using namespace std;

extern LocalRepo repository;

int
StatusDirectoryCB(void *arg, const char *path)
{
    string repoRoot = LocalRepo::findRootPath();
    string objPath = path;
    ObjectHash objHash;
    map<string, ObjectHash> *dirState = (map<string, ObjectHash> *)arg;

    if (!Util_IsDirectory(objPath)) {
	objHash = Util_HashFile(objPath);
        ASSERT(!objHash.isEmpty());
        objPath = objPath.substr(repoRoot.size());
    } else {
	objPath = objPath.substr(repoRoot.size());
    }

    // TODO: empty hash means dir
    dirState->insert(make_pair(objPath, objHash));

    return 0;
}

/*int
StatusTreeIter(map<string, pair<string, string> > *tipState,
	       const string &path,
	       const string &treeId)
{
    Tree tree;
    map<string, TreeEntry>::iterator it;

    // XXX: Error handling
    tree = repository.getTree(treeId);

    for (it = tree.tree.begin(); it != tree.tree.end(); it++) {
	if ((*it).second.type == TreeEntry::Tree) {
	    tipState->insert(make_pair(path + "/" + (*it).first,
				       make_pair("DIR", (*it).second.hash)));
	    StatusTreeIter(tipState,
			   path + "/" + (*it).first,
			   (*it).second.hash);
	} else {
            string filePath = path + "/" + (*it).first;
            string fileHash = (*it).second.largeHash == "" ?
                          (*it).second.hash : (*it).second.largeHash;
            string objHash = (*it).second.hash;

	    tipState->insert(make_pair(filePath,
                                       make_pair(fileHash, objHash)));
	}
    }

    return 0;
}*/

int
cmd_checkout(int argc, const char *argv[])
{
    Commit c;
    ObjectHash tip = repository.getHead();
    Tree::Flat tipTree;

    if (argc == 2) {
	tip = ObjectHash::fromHex(argv[1]);

	// Set the head if the user specified a revision
	repository.setHead(tip);
    }

    if (tip != EMPTY_COMMIT) {
        c = repository.getCommit(tip);
        tipTree = repository.getTree(c.getTree()).flattened(&repository);
    }

    map<string, ObjectHash> dirState;
    Scan_RTraverse(LocalRepo::findRootPath().c_str(),
		   (void *)&dirState,
	           StatusDirectoryCB);

    map<string, ObjectHash>::iterator it;
    for (it = dirState.begin(); it != dirState.end(); it++) {
        Tree::Flat::iterator tipIt = tipTree.find((*it).first);

	if (tipIt == tipTree.end()) {
	    printf("A	%s\n", (*it).first.c_str());
	} else {
            const TreeEntry &te = (*tipIt).second;
            ObjectHash totalHash = (te.type == TreeEntry::LargeBlob) ?
                    te.largeHash : te.hash;

            // TODO: dirs
            if (totalHash != (*it).second && !(*it).second.isEmpty()) {
                printf("M	%s\n", (*it).first.c_str());
                // XXX: Handle replace a file <-> directory with same name
                repository.copyObject(te.hash,
                                    LocalRepo::findRootPath()+(*tipIt).first);
            }
	}
    }

    for (Tree::Flat::iterator tipIt = tipTree.begin();
            tipIt != tipTree.end();
            tipIt++) {
	it = dirState.find((*tipIt).first);
	if (it == dirState.end()) {
	    string path = LocalRepo::findRootPath() + (*tipIt).first;
            const TreeEntry &te = (*tipIt).second;
	    if (te.type == TreeEntry::Tree) {
		printf("N	%s\n", (*tipIt).first.c_str());
		mkdir(path.c_str(), 0755);
	    } else {
		printf("U	%s\n", (*tipIt).first.c_str());
		if (repository.getObjectType(te.hash)
                        != ObjectInfo::Purged)
		    repository.copyObject(te.hash, path);
		else
		    cout << "Object has been purged." << endl;
	    }
	}
    }

    return 0;
}

