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
#include <stdlib.h>

#include <iostream>
#include <string>

#include <ori/commit.h>
#include <ori/oriutil.h>
#include <ori/stream.h>

using namespace std;

/********************************************************************
 *
 *
 * Commit
 *
 *
 ********************************************************************/

Commit::Commit()
    : message(), treeObjId(), user(), snapshotName(),
      date(0), graftRepo(), graftPath(), graftCommitId()
{
    parents.first.clear();
    parents.second.clear();
}

Commit::~Commit()
{
}

void
Commit::setParents(ObjectHash p1, ObjectHash p2)
{
    parents.first = p1;
    parents.second = p2;
}

pair<ObjectHash, ObjectHash>
Commit::getParents() const
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
Commit::setTree(const ObjectHash &tree)
{
    treeObjId = tree;
}

ObjectHash
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
Commit::setSnapshot(const std::string &snapshot)
{
    snapshotName = snapshot;
}

std::string
Commit::getSnapshot() const
{
    return snapshotName;
}

void
Commit::setTime(time_t t)
{
    date = t;
}

time_t
Commit::getTime() const
{
    return date;
}

void
Commit::setGraft(const string &repo,
		 const string &path,
		 const ObjectHash &commitId)
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

ObjectHash
Commit::getGraftCommit() const
{
    return graftCommitId;
}

string
Commit::getBlob() const
{
    strwstream ss;

    ss.writeHash(treeObjId);
    if (!parents.second.isEmpty()) {
        ss.writeInt<uint8_t>(2);
        ss.writeHash(parents.first);
        ss.writeHash(parents.second);
    }
    else if (!parents.first.isEmpty()) {
        ss.writeInt<uint8_t>(1);
        ss.writeHash(parents.first);
    }
    else {
        ss.writeInt<uint8_t>(0);
    }

    ss.writePStr(user);
    ss.writeInt<uint64_t>(date);
    ss.writePStr(snapshotName);

    if (graftRepo != "") {
	assert(graftPath != "");
	assert(!graftCommitId.isEmpty());

        ss.writeInt<uint8_t>(1);
        ss.writePStr(graftRepo);
        ss.writePStr(graftPath);
        ss.writeHash(graftCommitId);
    }
    else {
        ss.writeInt<uint8_t>(0);
    }

    ss.writePStr(message);

    return ss.str();
}

void
Commit::fromBlob(const string &blob)
{
    strstream ss(blob);

    ss.readHash(treeObjId);
    uint8_t numParents = ss.readInt<uint8_t>();
    if (numParents == 2) {
        ss.readHash(parents.first);
        ss.readHash(parents.second);
    }
    else if (numParents == 1) {
        ss.readHash(parents.first);
    }

    ss.readPStr(user);
    date = ss.readInt<uint64_t>();
    ss.readPStr(snapshotName);

    uint8_t hasGraft = ss.readInt<uint8_t>();
    if (hasGraft > 0) {
        ss.readPStr(graftRepo);
        ss.readPStr(graftPath);
        ss.readHash(graftCommitId);

	assert(graftPath != "");
	assert(!graftCommitId.isEmpty());
    }
    
    ss.readPStr(message);

    // Verify that everything is set!
}

ObjectHash
Commit::hash() const
{
    const std::string &blob = getBlob();
    ObjectHash h = Util_HashString(blob);
    /*fprintf(stderr, "Commit blob len %lu, hash %s\n", blob.size(),
            h.hex().c_str());
    Util_PrintHex(blob);*/
    return h;
}

void
Commit::print() const
{
    time_t timeVal = date;
    char timeStr[26];

    ctime_r(&timeVal, timeStr);

    cout << "Parents: "
	 << (parents.first.isEmpty() ? "" : parents.first.hex())
	 << " "
	 << (parents.second.isEmpty() ? "" : parents.second.hex()) << endl;
    cout << "Tree:    " << treeObjId.hex() << endl;
    cout << "Author:  " << user << endl;
    cout << "Date:    " << timeStr;
    if (!graftCommitId.isEmpty()) {
	cout << "Graft Repo: " << graftRepo << endl;
	cout << "Graft Path: " << graftPath << endl;
	cout << "Graft Commit: " << graftCommitId.hex() << endl;
    }
    cout << "----- BEGIN MESSAGE -----" << endl;
    cout << message << endl;
    cout << "----- END MESSAGE -----" << endl;
}

