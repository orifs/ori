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

#ifndef __REPO_H__
#define __REPO_H__

#include <stdint.h>

#include <string>
#include <set>

#include "tree.h"
#include "commit.h"
#include "object.h"

#define EMPTY_COMMIT "0000000000000000000000000000000000000000000000000000000000000000"

#define LARGEFILE_MINIMUM (1024 * 1024)

class Repo
{
public:
    Repo();
    virtual ~Repo();

    // Repo information
    // TODO: is this consistent with the model?
    virtual std::string getHead() = 0;

    // Objects
    virtual Object *getObject(
            const std::string &id
            ) = 0;
    virtual bool hasObject(const std::string &id) = 0;
    // TODO: add options to query?
    virtual std::set<ObjectInfo> listObjects() = 0;

    // TODO: add object interface
    virtual int addObjectRaw(const ObjectInfo &info, bytestream *bs) = 0;

    // High-level operations
    virtual void pull(Repo *r);

    virtual std::string addBlob(Object::Type type, const std::string &blob);
    virtual int addObject(
            const ObjectInfo &info,
            const std::string &payload
            );

    /*virtual int getData(
            ObjectInfo *info,
            std::string &data);*/
    virtual size_t getObjectLength(const std::string &objId);
    virtual Object::Type getObjectType(const std::string &objId);
    virtual Tree getTree(const std::string &treeId);
};

#endif /* __REPO_H__ */

