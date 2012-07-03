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

#include "tree.h"

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
Tree::getBlob()
{
    string blob = "";
    map<string, TreeEntry>::iterator it;

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

