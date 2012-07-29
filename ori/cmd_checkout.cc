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

#include "debug.h"
#include "scan.h"
#include "util.h"
#include "localrepo.h"

using namespace std;

extern LocalRepo repository;

int
StatusDirectoryCB(void *arg, const char *path)
{
    string repoRoot = LocalRepo::findRootPath();
    string objPath = path;
    string objHash;
    map<string, string> *dirState = (map<string, string> *)arg;

    if (!Util_IsDirectory(objPath)) {
	objHash = Util_HashFile(objPath);
        objPath = objPath.substr(repoRoot.size());

	dirState->insert(pair<string, string>(objPath, objHash));
    } else {
	objPath = objPath.substr(repoRoot.size());
	dirState->insert(pair<string, string>(objPath, "DIR"));
    }

    return 0;
}

int
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
}

int
cmd_checkout(int argc, const char *argv[])
{
    map<string, string> dirState;
    map<string, pair<string, string> > tipState;
    map<string, string>::iterator it;
    map<string, pair<string, string> >::iterator tipIt;
    Commit c;
    string tip = repository.getHead();

    if (argc == 2) {
	tip = argv[1];
    }

    if (tip != EMPTY_COMMIT) {
        c = repository.getCommit(tip);
	StatusTreeIter(&tipState, "", c.getTree());
    }

    Scan_RTraverse(LocalRepo::findRootPath().c_str(),
		   (void *)&dirState,
	           StatusDirectoryCB);

    for (it = dirState.begin(); it != dirState.end(); it++) {
	tipIt = tipState.find((*it).first);
	if (tipIt == tipState.end()) {
	    printf("A	%s\n", (*it).first.c_str());
	} else if ((*tipIt).second.first != (*it).second) {
	    printf("M	%s\n", (*it).first.c_str());
	    // XXX: Handle replace a file <-> directory with same name
	    assert((*it).second != "DIR");
	    repository.copyObject((*tipIt).second.second,
				  LocalRepo::findRootPath()+(*tipIt).first);
	}
    }

    for (tipIt = tipState.begin(); tipIt != tipState.end(); tipIt++) {
	it = dirState.find((*tipIt).first);
	if (it == dirState.end()) {
	    string path = LocalRepo::findRootPath() + (*tipIt).first;
	    if ((*tipIt).second.first == "DIR") {
		printf("N	%s\n", (*tipIt).first.c_str());
		mkdir(path.c_str(), 0755);
	    } else {
		printf("U	%s\n", (*tipIt).first.c_str());
		if (repository.getObjectType((*tipIt).second.second)
                        != Object::Purged)
		    repository.copyObject((*tipIt).second.second, path);
		else
		    cout << "Object has been purged." << endl;
	    }
	}
    }

    return 0;
}

