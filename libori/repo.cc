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
#include <set>
#include <queue>
#include <iostream>

#include "tuneables.h"
#include "debug.h"
#include "util.h"
#include "object.h"
#include "largeblob.h"

#include "repo.h"
#include "localrepo.h"
#include "sshrepo.h"

using namespace std;


ObjectHash EMPTY_COMMIT =
ObjectHash::fromHex("0000000000000000000000000000000000000000000000000000000000000000");
ObjectHash EMPTYFILE_HASH =
ObjectHash::fromHex("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

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

int
Repo::addObject(Object::Type type, const ObjectHash &hash, const string &payload)
{
    assert(!hash.isEmpty()); // hash must be included
    assert(type != Object::Null);

    ObjectInfo info(hash);
    info.type = type;
    info.payload_size = payload.size();
    assert(info.hasAllFields());

    if (info.getCompressed()) {
        bytestream *ss = new strstream(payload);
        lzmastream bs(ss, true);
        return addObjectRaw(info, &bs);
    }
    else {
        strstream ss(payload);
        return addObjectRaw(info, &ss);
    }
}

void
Repo::copyFrom(Object *other)
{
    bytestream::ap bs(other->getStoredPayloadStream());
    addObjectRaw(other->getInfo(), bs.get());
}

/*
 * Add a blob to the repository. This is a low-level interface.
 */
ObjectHash
Repo::addBlob(Object::Type type, const string &blob)
{
    ObjectHash hash = Util_HashString(blob);
    addObject(type, hash, blob);
    return hash;
}


/*
 * Add a file to the repository. This is a low-level interface.
 */
ObjectHash
Repo::addSmallFile(const string &path)
{
    diskstream ds(path);
    return addBlob(Object::Blob, ds.readAll());
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

    return make_pair(addBlob(Object::LargeBlob, blob), lb.totalHash);
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
    string blob = o->getPayload();

    assert(treeId == EMPTYFILE_HASH || o->getInfo().type == Object::Tree);

    Tree t;
    t.fromBlob(blob);

    return t;
}

Commit
Repo::getCommit(const ObjectHash &commitId)
{
    Object::sp o(getObject(commitId));
    string blob = o->getPayload();

    assert(commitId == EMPTYFILE_HASH || o->getInfo().type == Object::Commit);

    Commit c;
    if (blob.size() == 0) {
        printf("Error getting commit blob\n");
        assert(false);
        return c;
    }
    c.fromBlob(blob);

    return c;
}

