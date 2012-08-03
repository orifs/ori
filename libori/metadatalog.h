#ifndef __METADATALOG_H__
#define __METADATALOG_H__

#include <map>
#include <string>
#include <tr1/memory>

#include "objecthash.h"

typedef int32_t refcount_t;
typedef std::map<ObjectHash, refcount_t> RefcountMap;
typedef std::map<ObjectHash, std::string> MetadataMap;

class MetadataLog;
class MdTransaction
{
public:
    MdTransaction(MetadataLog *log);
    ~MdTransaction();
    typedef std::tr1::shared_ptr<MdTransaction> sp;

    void addRef(const ObjectHash &hash);
    void decRef(const ObjectHash &hash);
    void setMeta(const ObjectHash &hash, const std::string &data);

    // TODO: cancel this transaction
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

    bool open(const std::string &filename);
    /// rewrites the log file, optionally with new counts
    void rewrite(const RefcountMap *refs = NULL, const MetadataMap *data = NULL);

    void addRef(const ObjectHash &hash, MdTransaction::sp trs =
            MdTransaction::sp());
    refcount_t getRefCount(const ObjectHash &hash) const;
    std::string getMeta(const ObjectHash &hash) const;

    MdTransaction::sp begin();
    void commit(MdTransaction *tr);

private:
    friend class MdTransaction;
    int fd;
    RefcountMap refcounts;
    MetadataMap metadata;
};

#endif
