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

#ifndef __SNAPSHOTINDEX_H__
#define __SNAPSHOTINDEX_H__

#include <assert.h>

#include <string>
#include <map>

class SnapshotIndex
{
public:
    SnapshotIndex();
    ~SnapshotIndex();
    void open(const std::string &indexFile);
    void close();
    void rewrite();
    void addSnapshot(const std::string &name, const ObjectHash &commitId);
    void delSnapshot(const std::string &name);
    const ObjectHash &getSnapshot(const std::string &name) const;
    std::map<std::string, ObjectHash> getList();
private:
    int fd;
    std::string fileName;
    std::map<std::string, ObjectHash> snapshots;
};

#endif /* __SNAPSHOTINDEX_H__ */

