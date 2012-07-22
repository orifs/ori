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
Repo::addObject(Object::Type type, const string &hash, const string &payload)
{
    assert(hash != ""); // hash must be included
    assert(type != Object::Null);

    ObjectInfo info(hash.c_str());
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
string
Repo::addBlob(Object::Type type, const string &blob)
{
    string hash = Util_HashString(blob);
    addObject(type, hash, blob);
    return hash;
}


/*
 * Add a file to the repository. This is a low-level interface.
 */
string
Repo::addSmallFile(const string &path)
{
    string hash = Util_HashFile(path);
    string objPath;

    if (hash == "") {
	perror("Unable to hash file");
	return "";
    }

    diskstream ds(path);
    if (addObject(Object::Blob, hash, ds.readAll()) < 0) {
        perror("Unable to copy file");
        return "";
    }

    return hash;
}

/*
 * Add a file to the repository. This is a low-level interface.
 */
pair<string, string>
Repo::addLargeFile(const string &path)
{
    string blob;
    string hash;
    LargeBlob lb = LargeBlob(this);

    lb.chunkFile(path);
    blob = lb.getBlob();
    hash = Util_HashString(blob);

    if (!hasObject(hash)) {
        map<uint64_t, LBlobEntry>::iterator it;

        for (it = lb.parts.begin(); it != lb.parts.end(); it++) {
            addBackref(hash, (*it).second.hash);
        }
    }

    return make_pair(addBlob(Object::LargeBlob, blob), lb.hash);
}

/*
 * Add a file to the repository. This is an internal interface that pusheds the
 * work to addLargeFile or addSmallFile based on our size threshold.
 */
pair<string, string>
Repo::addFile(const string &path)
{
    size_t sz = Util_FileSize(path);

    if (sz > LARGEFILE_MINIMUM)
        return addLargeFile(path);
    else
        return make_pair(addSmallFile(path), "");
}




Tree
Repo::getTree(const std::string &treeId)
{
    Object::sp o(getObject(treeId));
    string blob = o->getPayload();

    assert(treeId == EMPTY_HASH || o->getInfo().type == Object::Tree);

    Tree t;
    t.fromBlob(blob);

    return t;
}

Commit
Repo::getCommit(const std::string &commitId)
{
    Object::sp o(getObject(commitId));
    string blob = o->getPayload();

    assert(commitId == EMPTY_HASH || o->getInfo().type == Object::Commit);

    Commit c;
    if (blob.size() == 0) {
        printf("Error getting commit blob\n");
        assert(false);
        return c;
    }
    c.fromBlob(blob);

    return c;
}

