/*
 * Copyright (c) 2012-2013 Stanford University
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

#ifndef __METADATALOG_H__
#define __METADATALOG_H__

#include <oriutil/objecthash.h>

typedef int32_t refcount_t;
typedef std::tr1::unordered_map<ObjectHash, refcount_t> RefcountMap;
typedef std::tr1::unordered_map<std::string, std::string> ObjMetadata;
typedef std::tr1::unordered_map<ObjectHash, ObjMetadata> MetadataMap;

class MetadataLog;
class MdTransaction
{
public:
    MdTransaction(MetadataLog *log);
    ~MdTransaction();
    typedef std::tr1::shared_ptr<MdTransaction> sp;

    void addRef(const ObjectHash &hash);
    void decRef(const ObjectHash &hash);
    void setMeta(const ObjectHash &hash, const std::string &key,
            const std::string &value);
private:
    friend class MetadataLog;
    MetadataLog *log;
    RefcountMap counts;
    MetadataMap metadata;
};

class MetadataLog
{
public:
    MetadataLog();
    ~MetadataLog();

    void open(const std::string &filename);
    void sync();
    /// rewrites the log file, optionally with new counts
    void rewrite(const RefcountMap *refs = NULL, const MetadataMap *data = NULL);

    void addRef(const ObjectHash &hash, MdTransaction::sp trs =
            MdTransaction::sp());
    refcount_t getRefCount(const ObjectHash &hash) const;
    std::string getMeta(const ObjectHash &hash, const std::string &key) const;

    MdTransaction::sp begin();
    void commit(MdTransaction *tr);

    void dumpRefs() const;
    void dumpMeta() const;

private:
    friend class MdTransaction;
    int fd;
    std::string filename;
    RefcountMap refcounts;
    MetadataMap metadata;
};

#endif
