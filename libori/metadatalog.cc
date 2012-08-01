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
        uint32_t num;
        size_t n = read(fd, &num, sizeof(uint32_t));
        readSoFar += sizeof(uint32_t);
        if (n == 0)
            break;
        if (n < 0) {
            perror("MetadataLog::open read");
            return false;
        }

        if ((num*68) + readSoFar > sb.st_size) {
            // TODO: check end of log for consistency
            printf("Corruption in this entry!\n");
            return false;
        }

        fprintf(stderr, "Reading %u metadata entries\n", num);
        for (size_t i = 0; i < num; i++) {
            std::string hash;
            hash.resize(64);
            read(fd, &hash[0], 64);

            refcount_t refcount;
            read(fd, &refcount, sizeof(refcount_t));

            refcounts[hash] = refcount;

            readSoFar += 64 + sizeof(refcount_t);
        }
    }

    return true;
}

void
MetadataLog::rewrite(const RefcountMap *refs)
{
    if (refs == NULL)
        refs = &refcounts;

    ftruncate(fd, 0);
    lseek(fd, 0, SEEK_SET);
    MdTransaction::sp tr = begin();
    tr->counts = *refs;

    refcounts.clear();
}

void
MetadataLog::addRef(const std::string &hash, MdTransaction::sp trs)
{
    if (!trs.get()) {
        trs = begin();
    }

    trs->counts[hash] += 1;
}

refcount_t
MetadataLog::getRefCount(const std::string &hash) const
{
    RefcountMap::const_iterator it = refcounts.find(hash);
    if (it == refcounts.end())
        return 0;
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
    uint32_t num = tr->counts.size();
    if (num == 0) return;
    
    fprintf(stderr, "Committing %u metadata entries\n", num);

    write(fd, &num, sizeof(uint32_t));

    strwstream ws(68*num + 4);
    for (RefcountMap::iterator it = tr->counts.begin();
            it != tr->counts.end();
            it++) {
        const std::string &hash = (*it).first;
        assert(hash.size() == 64);

        ws.write(hash.data(), hash.size());
        refcount_t final_count = refcounts[hash] + (*it).second;
        refcounts[hash] = final_count;
        ws.write(&final_count, sizeof(refcount_t));
    }

    std::string commitHash = Util_HashString(ws.str());
    //ws.write(commitHash.data(), commitHash.size());

    const std::string &str = ws.str();
    write(fd, str.data(), str.size());

    tr->counts.clear();
}
