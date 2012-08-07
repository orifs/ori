#ifndef __PACKFILE_H__
#define __PACKFILE_H__

#include <tr1/memory>

#include "objecthash.h"
#include "object.h"
#include "stream.h"

typedef uint32_t offset_t;

struct IndexEntry
{
    ObjectInfo info;
    offset_t offset;
    uint32_t packed_size;
    uint32_t packfile;

    const static size_t SIZE = ObjectInfo::SIZE + sizeof(offset_t) +
        2 * sizeof(uint32_t);
};

class PfTransaction
{
public:
    PfTransaction();
    ~PfTransaction();

    std::vector<ObjectInfo> infos;
    std::vector<offset_t> offsets;
    std::string payloads;
    bool committed;
};

class Index;
class Packfile
{
public:
    typedef std::tr1::shared_ptr<Packfile> sp;

    Packfile(const std::string &filename);
    ~Packfile();

    bool full() const;
    void addPayload(const ObjectInfo &info, bytestream *data_str, Index *idx);
    bytestream *getPayload(const IndexEntry &entry);
    void purge(const ObjectHash &hash);


    typedef void (*ReadEntryCb)(const ObjectInfo &info, offset_t off);
    void readEntries(ReadEntryCb cb);

    void transmit(bytewstream *bs, std::vector<IndexEntry> objects);
    void receive(bytestream *bs);

private:
    int fd;
    std::string filename;
    size_t numObjects;
    size_t fileSize;
};

#endif
