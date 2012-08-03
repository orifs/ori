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

#ifndef __INDEX_H__
#define __INDEX_H__

#include <assert.h>

#include <string>
#include <map>
#include <set>

#include "debug.h"
#include "object.h"

class Index
{
public:
    Index();
    ~Index();
    void open(const std::string &indexFile);
    void close();
    void rewrite();
    void dump();
    void updateInfo(const ObjectHash &objId, const ObjectInfo &info);
    const ObjectInfo &getInfo(const ObjectHash &objId) const;
    bool hasObject(const ObjectHash &objId) const;
    std::set<ObjectInfo> getList();
private:
    int fd;
    std::string fileName;
    std::map<ObjectHash, ObjectInfo> index;
};

#endif /* __INDEX_H__ */

