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

    fileName = StrUtil_Basename(fileName);
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

void
_recFlatten(
        const std::string &prefix,
        const Tree *t,
        map<string, TreeEntry> *rval,
        Repo *r
        )
{
    for (map<string, TreeEntry>::const_iterator it = t->tree.begin();
            it != t->tree.end();
            it++) {
        const TreeEntry &te = (*it).second;
        rval->insert(pair<string, TreeEntry>(prefix + (*it).first, te));
        if (te.type == TreeEntry::Tree) {
            // Recurse further
            Tree t = r->getTree(te.hash);
            _recFlatten(prefix + (*it).first + "/",
                    &t, rval, r);
        }
    }
}

map<string, TreeEntry>
Tree::flattened(Repo *r) const
{
    map<string, TreeEntry> rval;
    _recFlatten("/", this, &rval, r);
    return rval;
}




/*
 * TreeDiff
 */

void
TreeDiff::diffTwoTrees(const Tree &t1, const Tree &t2)
{
}

struct _scanHelperData {
    set<string> *wd_paths;
    map<string, TreeEntry> *flattened_tree;
    TreeDiff *td;

    size_t cwdLen;
    Repo *repo;
};

int _diffToWdHelper(void *arg, const char *path)
{
    string fullPath = path;
    _scanHelperData *sd = (_scanHelperData *)arg;

    string relPath = fullPath.substr(sd->cwdLen);
    sd->wd_paths->insert(relPath);

    TreeDiffEntry diffEntry;
    diffEntry.filepath = relPath;

    map<string, TreeEntry>::iterator it = sd->flattened_tree->find(relPath);
    if (it == sd->flattened_tree->end()) {
        // New file/dir
        if (Util_IsDirectory(fullPath)) {
            diffEntry.type = TreeDiffEntry::NewDir;
        }
        else {
            diffEntry.type = TreeDiffEntry::NewFile;
            diffEntry.newFilename = fullPath;
        }
        sd->td->entries.push_back(diffEntry);
        return 0;
    }

    // Potentially modified file/dir
    const TreeEntry &te = (*it).second;
    if (Util_IsDirectory(fullPath)) {
        if (te.type != TreeEntry::Tree) {
            // File replaced by dir
            diffEntry.type = TreeDiffEntry::Deleted;
            sd->td->entries.push_back(diffEntry);
            diffEntry.type = TreeDiffEntry::NewDir;
            sd->td->entries.push_back(diffEntry);
        }
        return 0;
    }

    if (te.type == TreeEntry::Tree) {
        // Dir replaced by file
        diffEntry.type = TreeDiffEntry::Deleted;
        sd->td->entries.push_back(diffEntry);
        diffEntry.type = TreeDiffEntry::NewFile;
        diffEntry.newFilename = fullPath;
        sd->td->entries.push_back(diffEntry);
        return 0;
    }

    // Check if file is modified
    // TODO: heuristic for determining whether a file has been modified
    std::string newHash = Util_HashFile(fullPath);
    bool modified = false;
    if (te.type == TreeEntry::Blob) {
        modified = newHash == te.hash;
    }
    else if (te.type == TreeEntry::LargeBlob) {
        modified = newHash == te.largeHash;
    }

    if (modified) {
        diffEntry.type = TreeDiffEntry::Modified;
        diffEntry.newFilename = fullPath;
        sd->td->entries.push_back(diffEntry);
        return 0;
    }

    return 0;
}

void
TreeDiff::diffToWD(Tree src, Repo *r)
{
    char *cwd = (char *)malloc(PATH_MAX);
    getcwd(cwd, PATH_MAX);

    set<string> wd_paths;
    map<string, TreeEntry> flattened_tree = src.flattened(r);
    _scanHelperData sd = {
        &wd_paths,
        &flattened_tree,
        this,
        strlen(cwd),
        r};

    // Find additions and modifications
    Scan_RTraverse(cwd, &sd, _diffToWdHelper);

    free(cwd);

    // Find deletions
    for (map<string, TreeEntry>::iterator it = flattened_tree.begin();
            it != flattened_tree.end();
            it++) {
        set<string>::iterator wd_it = wd_paths.find((*it).first);
        if (wd_it == wd_paths.end()) {
            TreeDiffEntry tde;
            tde.filepath = (*it).first;
            tde.type = TreeDiffEntry::Deleted;
            entries.push_back(tde);
        }
    }
}

