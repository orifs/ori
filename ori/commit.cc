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

#define _WITH_DPRINTF

#include <assert.h>
#include <stdio.h>

#include <iostream>
#include <sstream>
#include <string>

#include "commit.h"

using namespace std;

/********************************************************************
 *
 *
 * Commit
 *
 *
 ********************************************************************/

Commit::Commit()
{
}

Commit::~Commit()
{
}

void
Commit::setParents(std::string p1, std::string p2)
{
    parents.first = p1;
    parents.second = p2;
}

pair<string, string>
Commit::getParents()
{
    return parents;
}

void
Commit::setMessage(const string &msg)
{
    message = msg;
}

string
Commit::getMessage() const
{
    return message;
}

void
Commit::setTree(const string &tree)
{
    treeObjId = tree;
}

string
Commit::getTree() const
{
    return treeObjId;
}

void
Commit::setUser(const string &user)
{
    this->user = user;
}

string
Commit::getUser() const
{
    return user;
}

void
Commit::setGraft(const string &repo,
		 const string &path,
		 const string &commitId)
{
    graftRepo = repo;
    graftPath = path;
    graftCommitId = commitId;
}

pair<string, string>
Commit::getGraftRepo() const
{
    return make_pair(graftRepo, graftPath);
}

string
Commit::getGraftCommit() const
{
    return graftCommitId;
}

string
Commit::getBlob() const
{
    string blob;
    
    blob = "tree " + treeObjId + "\n";
    blob += "parent " + parents.first;
    if (parents.second != "") {
	blob += " " + parents.second;
    }
    blob += "\n";
    if (user != "") {
	blob += "user " + user + "\n";
    }
    if (graftRepo != "") {
	assert(graftPath != "");
	assert(graftCommitId != "");

	blob += "graft-repo " + graftRepo + "\n";
	blob += "graft-path " + graftPath + "\n";
	blob += "graft-commit " + graftCommitId + "\n";
    }
    blob += "\n";
    blob += message;

    return blob;
}

void
Commit::fromBlob(const string &blob)
{
    string line;
    stringstream ss(blob);

    while (getline(ss, line, '\n')) {
	if (line.substr(0, 5) == "tree ") {
	    treeObjId = line.substr(5);
	} else if (line.substr(0, 7) == "parent ") {
	    // XXX: Handle the merge case
	    parents.first = line.substr(7);
	} else if (line.substr(0, 5) == "user ") {
	    user = line.substr(5);
	} else if (line.substr(0, 11) == "graft-repo ") {
	    graftRepo = line.substr(11);
	} else if (line.substr(0, 11) == "graft-path ") {
	    graftPath = line.substr(11);
	} else if (line.substr(0, 13) == "graft-commit ") {
	    graftCommitId = line.substr(13);
	} else if (line.substr(0, 1) == "") {
	    message = blob.substr(ss.tellg());
	    break;
	} else{
	    printf("Unsupported commit parameter!\n");
	    assert(false);
	}
    }

    // Verify that everything is set!
}

