#include <stdint.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "debug.h"
#include "oriutil.h"
#include "stream.h"
#include "metadatalog.h"
#include "posixexception.h"

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
    ASSERT(log->refcounts[hash] + counts[hash] >= 0);
}

void MdTransaction::setMeta(const ObjectHash &hash, const std::string &key,
        const std::string &value)
{
    metadata[hash][key] = value;
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

    this->filename = filename;

    struct stat sb;
    if (fstat(fd, &sb) < 0) {
        perror("MetadataLog::open fstat");
        return false;
    }

    size_t readSoFar = 0;
    while (true) {
        uint32_t nbytes;
        int n = read(fd, &nbytes, sizeof(uint32_t));
        readSoFar += sizeof(uint32_t);
        if (n == 0)
            break;
        if (n < 0) {
            perror("MetadataLog::open read");
            return false;
        }

        if (readSoFar + nbytes > (size_t)sb.st_size) {
            // TODO: truncate this entry
            fprintf(stderr, "Corruption in this entry!\n");
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

        //fprintf(stderr, "Reading %u refcount entries\n", num_rc);
        for (size_t i = 0; i < num_rc; i++) {
            ObjectHash hash;
            ss.readHash(hash);

            refcount_t refcount = ss.readInt<refcount_t>();
            refcounts[hash] = refcount;
        }

        //fprintf(stderr, "Reading %u metadata entries\n", num_md);
        for (size_t i = 0; i < num_md; i++) {
            ObjectHash hash;
            ss.readHash(hash);

            uint32_t num_mde = ss.readInt<uint32_t>();
            for (size_t ix_mde = 0; ix_mde < num_mde; ix_mde++) {
                std::string key, value;
                ss.readPStr(key);
                ss.readPStr(value);
                metadata[hash][key] = value;
            }
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

    std::string tmpFilename = filename + ".tmp";
    int newFd = ::open(tmpFilename.c_str(), O_RDWR | O_CREAT | O_APPEND, 0644);
    if (newFd < 0) {
        perror("MetadataLog::rewrite open");
        throw PosixException(errno);
    }

    int oldFd = fd;
    fd = newFd;

    ftruncate(fd, 0);
    lseek(fd, 0, SEEK_SET);
    MdTransaction::sp tr = begin();
    tr->counts = *refs;
    tr->metadata = *data;

    refcounts.clear();
    metadata.clear();

    Util_RenameFile(tmpFilename, filename);
    ::close(oldFd);
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
MetadataLog::getMeta(const ObjectHash &hash, const std::string &key) const
{
    MetadataMap::const_iterator it = metadata.find(hash);
    if (it == metadata.end())
        return "";
    ObjMetadata::const_iterator mit = (*it).second.find(key);
    if (mit == (*it).second.end())
        return "";
    return (*mit).second;
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
    
    LOG("Committing %u refcount changes, %u metadata entries", num_rc, num_md);

    //write(fd, &num, sizeof(uint32_t));

    strwstream ws(36*num_rc + 8);
    ws.writeInt(num_rc);
    ws.writeInt(num_md);

    for (RefcountMap::iterator it = tr->counts.begin();
            it != tr->counts.end();
            it++) {
        const ObjectHash &hash = (*it).first;
        ASSERT(!hash.isEmpty());

        ws.writeHash(hash);
        refcount_t final_count = refcounts[hash] + (*it).second;
        ASSERT(final_count >= 0);

        refcounts[hash] = final_count;
        ws.writeInt(final_count);
    }

    for (MetadataMap::iterator it = tr->metadata.begin();
            it != tr->metadata.end();
            it++) {
        const ObjectHash &hash = (*it).first;
        ASSERT(!hash.isEmpty());

        ws.writeHash(hash);
        uint32_t num_mde = (*it).second.size();
        ws.writeInt(num_mde);

        for (ObjMetadata::iterator mit =
                (*it).second.begin();
                mit != (*it).second.end();
                mit++) {
            metadata[hash][(*mit).first] = (*mit).second;
            ws.writePStr((*mit).first);
            ws.writePStr((*mit).second);
        }
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
