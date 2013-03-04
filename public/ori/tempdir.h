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

#ifndef __TEMPDIR_H__
#define __TEMPDIR_H__

#include <set>
#include <vector>
#include <string>
#include <tr1/memory>
#include <tr1/unordered_map>

#include "repo.h"
#include "commit.h"
#include "index.h"

/** Class for managing a temporary directory of objects.
  * Uses an append-only log for keeping track of objects.
  */
class TempDir : public Repo
{
public:
    TempDir(const std::string &dirpath);
    ~TempDir();

    typedef std::tr1::shared_ptr<TempDir> sp;

    std::string pathTo(const std::string &file);
    /// Temp files are deleted along with the directory
    std::string newTempFile();
    
    // Repo implementation
    int distance() { return 0; }
    std::string getUUID() { NOT_IMPLEMENTED(false); }
    ObjectHash getHead() { NOT_IMPLEMENTED(false); }
    Object::sp getObject(const ObjectHash &objId);
    ObjectInfo getObjectInfo(const ObjectHash &objId);
    bool hasObject(const ObjectHash &objId);
    bytestream *getObjects(const ObjectHashVec &objs);
    std::set<ObjectInfo> listObjects();
    std::vector<Commit> listCommits() { NOT_IMPLEMENTED(false); }
    int addObject(ObjectType type, const ObjectHash &hash,
            const std::string &payload);

    std::string dirpath;

private:
    Index index;
    int objects_fd;
    std::tr1::unordered_map<ObjectHash, off_t> offsets;
};

class TempObject : public Object
{
public:
    TempObject(int fd, off_t off, size_t len, const ObjectInfo &info);

    bytestream *getPayloadStream();

private:
    int fd;
    off_t off;
    size_t len;
};

#endif
