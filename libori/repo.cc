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

#include <string>
#include <vector>
#include <set>
#include <queue>
#include <iostream>

#include "tuneables.h"

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/oricrypt.h>
#include <oriutil/dag.h>

#include <ori/object.h>
#include <ori/largeblob.h>
#include <ori/repo.h>
#include <ori/localrepo.h>
#include <ori/sshrepo.h>

using namespace std;


ObjectHash EMPTY_COMMIT =
ObjectHash::fromHex("0000000000000000000000000000000000000000000000000000000000000000");

#ifdef ORI_USE_SHA256
ObjectHash EMPTYFILE_HASH =
ObjectHash::fromHex("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
#endif
#ifdef ORI_USE_SKEIN
ObjectHash EMPTYFILE_HASH =
ObjectHash::fromHex("c8877087da56e072870daa843f176e9453115929094c3a40c463a196c29bf7ba");
#endif

/*
 * Repo
 */

Repo::Repo() {
}

Repo::~Repo() {
}

/*
 * High-level operations
 */

void
Repo::copyFrom(Object *other)
{
    addObject(other->getInfo().type, other->getInfo().hash, other->getPayload());
}

/*
 * Add a blob to the repository. This is a low-level interface.
 */
ObjectHash
Repo::addBlob(ObjectType type, const string &blob)
{
    ObjectHash hash = OriCrypt_HashString(blob);
    addObject(type, hash, blob);
    return hash;
}


bytestream *
Repo::getObjects(const std::deque<ObjectHash> &objs)
{
    ObjectHashVec vec;
    for (size_t i = 0; i < objs.size(); i++) {
        vec.push_back(objs[i]);
    }
    return getObjects(vec);
}


/*
 * Add a file to the repository. This is a low-level interface.
 */
ObjectHash
Repo::addSmallFile(const string &path)
{
    diskstream ds(path);
    return addBlob(ObjectInfo::Blob, ds.readAll());
}

/*
 * Add a file to the repository. This is a low-level interface.
 */
pair<ObjectHash, ObjectHash>
Repo::addLargeFile(const string &path)
{
    string blob;
    string hash;
    LargeBlob lb = LargeBlob(this);

    lb.chunkFile(path);
    blob = lb.getBlob();

    // TODO: this should only be called when committing,
    // we'll take care of backrefs then
    /*if (!hasObject(hash)) {
        map<uint64_t, LBlobEntry>::iterator it;

        for (it = lb.parts.begin(); it != lb.parts.end(); it++) {
            addBackref((*it).second.hash);
        }
    }*/

    return make_pair(addBlob(ObjectInfo::LargeBlob, blob), lb.totalHash);
}

/*
 * Add a file to the repository. This is an internal interface that pusheds the
 * work to addLargeFile or addSmallFile based on our size threshold.
 */
pair<ObjectHash, ObjectHash>
Repo::addFile(const string &path)
{
    size_t sz = Util_FileSize(path);

    if (sz > LARGEFILE_MINIMUM)
        return addLargeFile(path);
    else
        return make_pair(addSmallFile(path), ObjectHash());
}




Tree
Repo::getTree(const ObjectHash &treeId)
{
    Object::sp o(getObject(treeId));
    if (!o.get()) {
        throw std::runtime_error("Object not found");
    }
    string blob = o->getPayload();

    ASSERT(treeId == EMPTYFILE_HASH || o->getInfo().type == ObjectInfo::Tree);

    Tree t;
    t.fromBlob(blob);

    return t;
}

Commit
Repo::getCommit(const ObjectHash &commitId)
{
    Object::sp o(getObject(commitId));
    string blob = o->getPayload();

    ASSERT(commitId == EMPTYFILE_HASH || o->getInfo().type == ObjectInfo::Commit);

    Commit c;
    if (blob.size() == 0) {
        printf("Error getting commit blob\n");
        PANIC();
        return c;
    }
    c.fromBlob(blob);

    return c;
}

DAG<ObjectHash, Commit>
Repo::getCommitDag()
{
    vector<Commit> commits = listCommits();
    vector<Commit>::iterator it;
    DAG<ObjectHash, Commit> cDag = DAG<ObjectHash, Commit>();

    cDag.addNode(ObjectHash(), Commit());
    for (it = commits.begin(); it != commits.end(); it++) {
	cDag.addNode((*it).hash(), (*it));
    }

    for (it = commits.begin(); it != commits.end(); it++) {
	pair<ObjectHash, ObjectHash> p = (*it).getParents();
	cDag.addEdge(p.first, (*it).hash());
	if (!p.second.isEmpty())
	    cDag.addEdge(p.first, it->hash());
    }

    return cDag;
}

