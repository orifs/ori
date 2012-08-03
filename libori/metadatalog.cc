#include <cassert>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "metadatalog.h"
#include "util.h"
#include "stream.h"

MdTransaction::MdTransaction(MetadataLog *log)
    : log(log)
{
}

MdTransaction::~MdTransaction()
{
    if (log != NULL) {
        log->commit(this);
    }
}

void MdTransaction::addRef(const ObjectHash &hash)
{
    counts[hash] += 1;
}

void MdTransaction::decRef(const ObjectHash &hash)
{
    counts[hash] -= 1;
    assert(log->refcounts[hash] + counts[hash] >= 0);
}

void MdTransaction::setMeta(const ObjectHash &hash, const std::string &data)
{
    metadata[hash] = data;
}



/*
 * MetadataLog
 */

MetadataLog::MetadataLog()
    : fd(-1)
{
}

MetadataLog::~MetadataLog()
{
    if (fd != -1) {
        ::close(fd);
    }
}

bool
MetadataLog::open(const std::string &filename)
{
    fd = ::open(filename.c_str(), O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("MetadataLog::open open");
        return false;
    }

    struct stat sb;
    if (fstat(fd, &sb) < 0) {
        perror("MetadataLog::open fstat");
        return false;
    }

    // TODO: read log
    size_t readSoFar = 0;
    while (true) {
        uint32_t nbytes;
        size_t n = read(fd, &nbytes, sizeof(uint32_t));
        readSoFar += sizeof(uint32_t);
        if (n == 0)
            break;
        if (n < 0) {
            perror("MetadataLog::open read");
            return false;
        }

        if (readSoFar + nbytes > sb.st_size) {
            // TODO: check end of log for consistency
            printf("Corruption in this entry!\n");
            return false;
        }

        std::string packet;
        packet.resize(nbytes);
        if (read(fd, &packet[0], nbytes) < 0) {
            perror("MetadataLog::open read");
            return false;
        }
        readSoFar += nbytes;

        strstream ss(packet);
        uint32_t num_rc = ss.readInt<uint32_t>();
        uint32_t num_md = ss.readInt<uint32_t>();

        fprintf(stderr, "Reading %u refcount entries\n", num_rc);
        for (size_t i = 0; i < num_rc; i++) {
            ObjectHash hash;
            ss.readHash(hash);

            refcount_t refcount = ss.readInt<refcount_t>();
            refcounts[hash] = refcount;
        }

        fprintf(stderr, "Reading %u metadata entries\n", num_md);
        for (size_t i = 0; i < num_md; i++) {
            ObjectHash hash;
            ss.readHash(hash);

            std::string data;
            ss.readPStr(data);
            metadata[hash] = data;
        }
    }

    return true;
}

void
MetadataLog::rewrite(const RefcountMap *refs, const MetadataMap *data)
{
    if (refs == NULL)
        refs = &refcounts;
    if (data == NULL)
        data = &metadata;

    // TODO: use rename
    ftruncate(fd, 0);
    lseek(fd, 0, SEEK_SET);
    MdTransaction::sp tr = begin();
    tr->counts = *refs;
    tr->metadata = *data;

    refcounts.clear();
    metadata.clear();
}

void
MetadataLog::addRef(const ObjectHash &hash, MdTransaction::sp trs)
{
    if (!trs.get()) {
        trs = begin();
    }

    trs->addRef(hash);
}

refcount_t
MetadataLog::getRefCount(const ObjectHash &hash) const
{
    RefcountMap::const_iterator it = refcounts.find(hash);
    if (it == refcounts.end())
        return 0;
    return (*it).second;
}

std::string
MetadataLog::getMeta(const ObjectHash &hash) const
{
    MetadataMap::const_iterator it = metadata.find(hash);
    if (it == metadata.end())
        return "";
    return (*it).second;
}

MdTransaction::sp
MetadataLog::begin()
{
    return MdTransaction::sp(new MdTransaction(this));
}

void
MetadataLog::commit(MdTransaction *tr)
{
    uint32_t num_rc = tr->counts.size();
    uint32_t num_md = tr->metadata.size();
    if (num_rc + num_md == 0) return;
    
    fprintf(stderr, "Committing %u refcount changes, %u metadata entries\n",
            num_rc, num_md);

    //write(fd, &num, sizeof(uint32_t));

    strwstream ws(36*num_rc + 8);
    ws.writeInt(num_rc);
    ws.writeInt(num_md);

    for (RefcountMap::iterator it = tr->counts.begin();
            it != tr->counts.end();
            it++) {
        const ObjectHash &hash = (*it).first;
        assert(!hash.isEmpty());

        ws.writeHash(hash);
        refcount_t final_count = refcounts[hash] + (*it).second;
        assert(final_count >= 0);

        refcounts[hash] = final_count;
        ws.writeInt(final_count);
    }

    for (MetadataMap::iterator it = tr->metadata.begin();
            it != tr->metadata.end();
            it++) {
        const ObjectHash &hash = (*it).first;
        assert(!hash.isEmpty());

        ws.writeHash(hash);
        metadata[hash] = (*it).second;
        ws.writePStr((*it).second);
    }

    //ObjectHash commitHash = Util_HashString(ws.str());
    //ws.write(commitHash.data(), commitHash.size());

    const std::string &str = ws.str();
    uint32_t nbytes = str.size();
    write(fd, &nbytes, sizeof(uint32_t));
    write(fd, str.data(), str.size());

    tr->counts.clear();
    tr->metadata.clear();
}
