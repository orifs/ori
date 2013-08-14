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
#include <string.h>

#include <fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <set>
#include <iostream>
#include <boost/tr1/unordered_map.hpp>

#include <oriutil/debug.h>
#include <oriutil/runtimeexception.h>
#include <oriutil/systemexception.h>
#include <oriutil/orifile.h>
#include <oriutil/oricrypt.h>
#include <ori/object.h>
#include <ori/index.h>

using namespace std;
using namespace std::tr1;

/// Adds a checksum
#define TOTAL_ENTRYSIZE (IndexEntry::SIZE + 16)

Index::Index()
{
    fd = -1;
}

Index::~Index()
{
    close();
}

void
Index::open(const string &indexFile)
{
    int i, entries;
    struct stat sb;

    fileName = indexFile;

    // Read index
    fd = ::open(indexFile.c_str(), O_RDWR | O_CREAT,
              S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        WARNING("Could not open the index file!");
        throw SystemException();
    }


    if (::fstat(fd, &sb) < 0) {
        int errcode = errno;
        ::close(fd);
        fd = -1;
        WARNING("Could not fstat the index file!");
        throw SystemException(errcode);
    }

    if (sb.st_size % TOTAL_ENTRYSIZE != 0) {
        // XXX: Attempt truncating last entries
        WARNING("Index seems dirty please rebuild it!");
        ::close(fd);
        fd = -1;
        throw RuntimeException(ORIEC_INDEXDIRTY, "Index dirty");
    }

    entries = sb.st_size / TOTAL_ENTRYSIZE;
    for (i = 0; i < entries; i++) {
        std::string entry_str(TOTAL_ENTRYSIZE, '\0');

        int status UNUSED = read(fd, &entry_str[0], TOTAL_ENTRYSIZE);
        ASSERT(status == TOTAL_ENTRYSIZE);

        IndexEntry entry;

        string info_str = entry_str.substr(0, ObjectInfo::SIZE);
        entry.info.fromString(info_str);

        strstream ss(entry_str, ObjectInfo::SIZE);
        entry.offset = ss.readUInt32();
        entry.packed_size = ss.readUInt32();
        entry.packfile = ss.readUInt32();


        std::vector<uint8_t> storedChecksum(16);
        ss.read(&storedChecksum[0], 16);
        ObjectHash computedChecksum =
            OriCrypt_HashString(entry_str.substr(0, IndexEntry::SIZE));
        if (memcmp(&storedChecksum[0], computedChecksum.hash, 16) != 0) {
            // XXX: Attempt truncating last entries
            WARNING("Index has corrupt entries please rebuild it!");
            ::close(fd);
            fd = -1;
            throw RuntimeException(ORIEC_INDEXCORRUPT, "Index corrupt");
        }

        index[entry.info.hash] = entry;
    }
    ::close(fd);

    // Reopen append only
    fd = ::open(indexFile.c_str(), O_WRONLY | O_APPEND);
    ASSERT(fd >= 0); // Assume that the repository lock protects the index

    // Delete temporary index if present
    if (OriFile_Exists(indexFile + ".tmp")) {
        OriFile_Delete(indexFile + ".tmp");
    }
}

void
Index::close()
{
    if (fd != -1) {
        ::fsync(fd);
        ::close(fd);
        fd = -1;
    }
}

void
Index::sync()
{
    ::fsync(fd);
}

void
Index::rewrite()
{
    int fdNew;
    string newIndex = fileName + ".tmp";

    fdNew = ::open(newIndex.c_str(), O_RDWR | O_CREAT,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fdNew < 0) {
        perror("open");
        WARNING("Could not open a temporary index file!");
        return;
    };

    int tmpFd = fd;
    fd = fdNew;
    ::close(tmpFd);

    // Write new index
    for (unordered_map<ObjectHash, IndexEntry>::iterator it = index.begin();
            it != index.end();
            it++)
    {
        _writeEntry((*it).second);
    }

    OriFile_Rename(newIndex, fileName);
}

void
Index::dump()
{
    unordered_map<ObjectHash, IndexEntry>::iterator it;

    cout << "***** BEGIN REPOSITORY INDEX *****" << endl;
    for (it = index.begin(); it != index.end(); it++)
    {
        cout << (*it).first.hex() << " packfile: " << (*it).second.packfile <<
            "," << (*it).second.offset << endl;
    }
    cout << "***** END REPOSITORY INDEX *****" << endl;
}

void
Index::updateEntry(const ObjectHash &objId, const IndexEntry &entry)
{
    ASSERT(!objId.isEmpty());

    _writeEntry(entry);

    if (index.find(objId) != index.end()) {
        fprintf(stderr, "WARNING: duplicate updateEntry\n");
    }

    // Add to in-memory index
    index[objId] = entry;
}

const IndexEntry &
Index::getEntry(const ObjectHash &objId) const
{
    unordered_map<ObjectHash, IndexEntry>::const_iterator it = index.find(objId);
    ASSERT(it != index.end());

    return (*it).second;
}

const ObjectInfo &
Index::getInfo(const ObjectHash &objId) const
{
    return getEntry(objId).info;
}

bool
Index::hasObject(const ObjectHash &objId) const
{
    unordered_map<ObjectHash, IndexEntry>::const_iterator it;

    it = index.find(objId);

    return it != index.end();
}

set<ObjectInfo>
Index::getList()
{
    set<ObjectInfo> lst;
    unordered_map<ObjectHash, IndexEntry>::iterator it;

    for (it = index.begin(); it != index.end(); it++)
    {
        lst.insert((*it).second.info);
    }

    return lst;
}


void
Index::_writeEntry(const IndexEntry &e)
{
    strwstream ss;

    string info_str = e.info.toString();
    ss.write(info_str.data(), info_str.size());

    ss.writeUInt32(e.offset);
    ss.writeUInt32(e.packed_size);
    ss.writeUInt32(e.packfile);

    ObjectHash checksum = OriCrypt_HashString(ss.str());
    ss.write(checksum.hash, 16);

    const string &final = ss.str();
    ASSERT(final.size() == TOTAL_ENTRYSIZE);
    write(fd, final.data(), final.size());
}

