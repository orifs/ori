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
#include <stdio.h>

#include <unistd.h>
#include <sys/stat.h>

#include <iostream>
#include <sstream>
#include <string>
#include <set>

#include "tree.h"
#include "util.h"
#include "scan.h"
#include "repo.h"

using namespace std;

/********************************************************************
 *
 *
 * TreeEntry
 *
 *
 ********************************************************************/

TreeEntry::TreeEntry()
{
    mode = Null;
}

TreeEntry::~TreeEntry()
{
}

/********************************************************************
 *
 *
 * Tree
 *
 *
 ********************************************************************/

Tree::Tree()
{
}

Tree::~Tree()
{
}

void
Tree::addObject(const char *path, const string &objId, const string &lgObjId)
{
    string fileName = path;
    struct stat sb;
    TreeEntry entry = TreeEntry();

    if (stat(path, &sb) < 0) {
	perror("stat failed in Tree::addObject");
	assert(false);
    }

    if (sb.st_mode & S_IFDIR) {
	entry.type = TreeEntry::Tree;
    } else {
	entry.type = TreeEntry::Blob;
    }

    entry.mode = sb.st_mode & (S_IRWXO | S_IRWXG | S_IRWXU | S_IFDIR);
    entry.hash = objId;

    if (lgObjId != "") {
        assert(entry.type == TreeEntry::Blob);
        entry.type = TreeEntry::LargeBlob;
        entry.largeHash = lgObjId;
    }

    fileName = fileName.substr(fileName.rfind("/") + 1);
    tree[fileName] = entry;
}


const string
Tree::getBlob() const
{
    string blob = "";
    map<string, TreeEntry>::const_iterator it;

    for (it = tree.begin(); it != tree.end(); it++)
    {
        if ((*it).second.type == TreeEntry::Tree) {
            blob += "tree";
        } else if ((*it).second.type == TreeEntry::Blob) {
            blob += "blob";
        } else if ((*it).second.type == TreeEntry::LargeBlob) {
            blob += "lgbl";
        } else {
            assert(false);
        }
	blob += " ";
	blob += (*it).second.hash;
        if ((*it).second.type == TreeEntry::LargeBlob) {
            blob += (*it).second.largeHash;
        }
	blob += " ";
	blob += (*it).first;
	blob += "\n";
    }

    return blob;
}

void
Tree::fromBlob(const string &blob)
{
    string line;
    stringstream ss(blob);

    while (getline(ss, line, '\n')) {
	TreeEntry entry = TreeEntry();
	if (line.substr(0, 5) == "tree ") {
	    entry.type = TreeEntry::Tree;
	} else if (line.substr(0, 5) == "blob ") {
	    entry.type = TreeEntry::Blob;
	} else if (line.substr(0, 5) == "lgbl ") {
	    entry.type = TreeEntry::LargeBlob;
	} else {
	    assert(false);
	}

	// XXX: entry.mode

        entry.hash = line.substr(5, 64);
        if (entry.type == TreeEntry::LargeBlob) {
            entry.largeHash = line.substr(69, 64);
	    tree[line.substr(134)] = entry;
        } else {
            entry.largeHash = "";
	    tree[line.substr(70)] = entry;
        }
    }
}




/*
 * TreeDiff
 */

void
TreeDiff::diffTwoTrees(const Tree &t1, const Tree &t2)
{
}

struct _scanHelperData {
    const Tree *t1, *t2;
    TreeDiff *td;

    set<string> *pathset;
    size_t cwdLen;
    Repo *repo;
};

int _diffToWdHelper(void *arg, const char *path)
{
    string fullPath = path;
    _scanHelperData *sd = (_scanHelperData *)arg;
    const Tree *tree = sd->t1;

    string relPath = fullPath.substr(sd->cwdLen);
    sd->pathset->insert(relPath);

    TreeDiffEntry diffEntry;
    diffEntry.filepath = relPath;

    if (tree == NULL) {
        // Every file/dir is new
        if (!Util_IsDirectory(fullPath)) {
            diffEntry.type = TreeDiffEntry::NewFile;
            sd->td->entries.push_back(diffEntry);
        }
        else {
            diffEntry.type = TreeDiffEntry::NewDir;
            sd->td->entries.push_back(diffEntry);
            Scan_Traverse(path, sd, _diffToWdHelper);
        }
        return 0;
    }

    string filename = StrUtil_Basename(relPath);
    map<string, TreeEntry>::const_iterator it = tree->tree.find(filename);

    if (Util_IsDirectory(fullPath)) {
        if (it == tree->tree.end()) {
            // New dir
            diffEntry.type = TreeDiffEntry::NewDir;
            sd->td->entries.push_back(diffEntry);

            _scanHelperData new_sd = *sd;
            new_sd.t1 = NULL;
            return Scan_Traverse(path, &new_sd, _diffToWdHelper);
        }
        else {
            const TreeEntry &te = (*it).second;
            if (te.type != TreeEntry::Tree) {
                // TODO: was a file
                assert(false);
            }
            _scanHelperData new_sd = *sd;
            Tree new_tree = sd->repo->getTree(te.hash);
            new_sd.t1 = &new_tree;
            return Scan_Traverse(path, &new_sd, _diffToWdHelper);
        }

        return 0;
    }

    if (it == tree->tree.end()) {
        // New file
        diffEntry.type = TreeDiffEntry::NewFile;
        sd->td->entries.push_back(diffEntry);
    }
    else {
        if ((*it).second.type == TreeEntry::Tree) {
            // TODO: was a dir
            assert(false);
        }

        string fileHash = Util_HashFile(fullPath);
        if (fileHash != (*it).second.hash) {
            // Modified
            diffEntry.type = TreeDiffEntry::Modified;
            sd->td->entries.push_back(diffEntry);
        }
    }

    return 0;
}

void
TreeDiff::diffToWD(Tree src, Repo *r)
{
    char *cwd = (char *)malloc(PATH_MAX);
    getcwd(cwd, PATH_MAX);

    set<string> pathset;
    _scanHelperData sd = {&src, NULL, this, &pathset, strlen(cwd), r};
    Scan_Traverse(cwd, &sd, _diffToWdHelper);

    free(cwd);

    for (size_t i = 0; i < entries.size(); i++) {
        printf("%c %s\n", entries[i].type, entries[i].filepath.c_str());
    }
}

