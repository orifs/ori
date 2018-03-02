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

#include <stdint.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <string>
#include <iostream>
#include <memory>
#include <unordered_map>

#include <oriutil/debug.h>
#include <oriutil/orifile.h>
#include <oriutil/stream.h>
#include <oriutil/systemexception.h>
#include <ori/metadatalog.h>

using namespace std;

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

void MdTransaction::setMeta(const ObjectHash &hash, const string &key,
        const string &value)
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

// XXX: Handle crach detection and recovery
void
MetadataLog::open(const string &filename)
{
    fd = ::open(filename.c_str(), O_RDWR | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        WARNING("MetadataLog open failed!");
        throw SystemException();
    }

    this->filename = filename;

    struct stat sb;
    if (fstat(fd, &sb) < 0) {
        WARNING("MetadataLog fstat failed!");
        throw SystemException();
    }

    size_t readSoFar = 0;
    while (true) {
        uint32_t nbytes;
        int n = read(fd, &nbytes, sizeof(uint32_t));
        readSoFar += sizeof(uint32_t);
        if (n == 0)
            break;
        if (n < 0) {
            WARNING("MetadataLog read failed!");
            throw SystemException();
        }

        if (readSoFar + nbytes > (size_t)sb.st_size) {
            // TODO: truncate this entry
            WARNING("Corrupted metadata log entry!");
            throw SystemException();
        }

        string packet;
        packet.resize(nbytes);
        if (read(fd, &packet[0], nbytes) < 0) {
            WARNING("MetadataLog read failed!");
            throw SystemException();
        }
        readSoFar += nbytes;

        strstream ss(packet);
        uint32_t num_rc = ss.readUInt32();
        uint32_t num_md = ss.readUInt32();

        //fprintf(stderr, "Reading %u refcount entries\n", num_rc);
        for (size_t i = 0; i < num_rc; i++) {
            ObjectHash hash;
            ss.readHash(hash);

            refcount_t refcount = ss.readInt32();
            refcounts[hash] = refcount;
        }

        //fprintf(stderr, "Reading %u metadata entries\n", num_md);
        for (size_t i = 0; i < num_md; i++) {
            ObjectHash hash;
            ss.readHash(hash);

            uint32_t num_mde = ss.readUInt32();
            for (size_t ix_mde = 0; ix_mde < num_mde; ix_mde++) {
                string key, value;
                ss.readPStr(key);
                ss.readPStr(value);
                metadata[hash][key] = value;
            }
        }
    }
}

void
MetadataLog::sync()
{
    ::fsync(fd);
}

void
MetadataLog::rewrite(const RefcountMap *refs, const MetadataMap *data)
{
    if (refs == NULL)
        refs = &refcounts;
    if (data == NULL)
        data = &metadata;

    string tmpFilename = filename + ".tmp";
    int newFd = ::open(tmpFilename.c_str(), O_RDWR | O_CREAT | O_APPEND, 0644);
    if (newFd < 0) {
        perror("MetadataLog::rewrite open");
        throw SystemException();
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

    OriFile_Rename(tmpFilename, filename);
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

string
MetadataLog::getMeta(const ObjectHash &hash, const string &key) const
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
    
    DLOG("Committing %u refcount changes, %u metadata entries", num_rc, num_md);

    //write(fd, &num, sizeof(uint32_t));

    strwstream ws(36*num_rc + 8);
    ws.writeUInt32(num_rc);
    ws.writeUInt32(num_md);

    for (RefcountMap::iterator it = tr->counts.begin();
            it != tr->counts.end();
            it++) {
        const ObjectHash &hash = (*it).first;
        ASSERT(!hash.isEmpty());

        ws.writeHash(hash);
        refcount_t final_count = refcounts[hash] + (*it).second;
        ASSERT(final_count >= 0);

        refcounts[hash] = final_count;
        ws.writeInt32(final_count);
    }

    for (MetadataMap::iterator it = tr->metadata.begin();
            it != tr->metadata.end();
            it++) {
        const ObjectHash &hash = (*it).first;
        ASSERT(!hash.isEmpty());

        ws.writeHash(hash);
        uint32_t num_mde = (*it).second.size();
        ws.writeUInt32(num_mde);

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

    const string &str = ws.str();
    uint32_t nbytes = str.size();
    write(fd, &nbytes, sizeof(uint32_t));
    write(fd, str.data(), str.size());

    tr->counts.clear();
    tr->metadata.clear();
}

void
MetadataLog::dumpRefs() const
{
    RefcountMap::const_iterator it;

    cout << "Reference Counts:" << endl;
    for (it = refcounts.begin(); it != refcounts.end(); it++)
    {
        cout << (*it).first.hex() << ": " << (*it).second << endl;
    }
}

void
MetadataLog::dumpMeta() const
{
    MetadataMap::const_iterator it;

    cout << "Metadata:" << endl;
    for (it = metadata.begin(); it != metadata.end(); it++)
    {
        ObjMetadata::const_iterator mit;

        cout << (*it).first.hex() << ":" << endl;

        for (mit = (*it).second.begin(); mit != (*it).second.end(); mit++)
        {
            cout << "  " << (*mit).first << " = " << (*mit).second << endl;
        }
    }
}

