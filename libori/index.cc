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

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <map>
#include <set>
#include <iostream>

#include "debug.h"
#include "util.h"
#include "object.h"
#include "index.h"

using namespace std;

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
        perror("open");
        cout << "Could not open the index file!" << endl;
        assert(false);
        return;
    }


    if (::fstat(fd, &sb) < 0) {
        perror("fstat");
        return;
    }

    if (sb.st_size % TOTAL_ENTRYSIZE != 0) {
	// XXX: Attempt truncating last entries
        cout << "Index seems dirty please rebuild it!" << endl;
        ::close(fd);
        fd = -1;
        exit(1);
        return;
    }

    entries = sb.st_size / TOTAL_ENTRYSIZE;
    for (i = 0; i < entries; i++) {
        std::string entry_str(TOTAL_ENTRYSIZE, '\0');

        int status = read(fd, &entry_str[0], TOTAL_ENTRYSIZE);
        assert(status == TOTAL_ENTRYSIZE);

        IndexEntry entry;

        string info_str = entry_str.substr(0, ObjectInfo::SIZE);
        entry.info.fromString(info_str);

        strstream ss(entry_str, ObjectInfo::SIZE);
        entry.offset = ss.readInt<offset_t>();
        entry.packed_size = ss.readInt<uint32_t>();
        entry.packfile = ss.readInt<uint32_t>();


        std::vector<uint8_t> storedChecksum;
        ss.read(&storedChecksum[0], 16);
        ObjectHash computedChecksum =
            Util_HashString(entry_str.substr(0, IndexEntry::SIZE));
        if (memcmp(&storedChecksum[0], computedChecksum.hash, 16) != 0) {
	    // XXX: Attempt truncating last entries
	    cout << "Index has corrupt entries please rebuild it!" << endl;
	    ::close(fd);
	    fd = -1;
	    exit(1);
	    return;
        }

        index[entry.info.hash] = entry;
    }
    ::close(fd);

    // Reopen append only
    fd = ::open(indexFile.c_str(), O_WRONLY | O_APPEND);
    assert(fd >= 0); // Assume that the repository lock protects the index

    // Delete temporary index if present
    if (Util_FileExists(indexFile + ".tmp")) {
        Util_DeleteFile(indexFile + ".tmp");
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
Index::rewrite()
{
    int fdNew;
    string newIndex = fileName + ".tmp";

    fdNew = ::open(newIndex.c_str(), O_RDWR | O_CREAT,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fdNew < 0) {
        perror("open");
        cout << "Could not open a temporary index file!" << endl;
        return;
    };

    int tmpFd = fd;
    fd = fdNew;
    ::close(tmpFd);

    // Write new index
    for (map<ObjectHash, IndexEntry>::iterator it = index.begin();
            it != index.end();
            it++)
    {
        _writeEntry((*it).second);
    }

    Util_RenameFile(newIndex, fileName);
}

void
Index::dump()
{
    map<ObjectHash, IndexEntry>::iterator it;

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
    assert(!objId.isEmpty());

    _writeEntry(entry);

    // Add to in-memory index
    index[entry.info.hash] = entry;
}

const IndexEntry &
Index::getEntry(const ObjectHash &objId) const
{
    map<ObjectHash, IndexEntry>::const_iterator it = index.find(objId);
    assert(it != index.end());

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
    map<ObjectHash, IndexEntry>::const_iterator it;

    it = index.find(objId);

    return it != index.end();
}

set<ObjectInfo>
Index::getList()
{
    set<ObjectInfo> lst;
    map<ObjectHash, IndexEntry>::iterator it;

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

    ss.writeInt(e.offset);
    ss.writeInt(e.packed_size);
    ss.writeInt(e.packfile);

    ObjectHash checksum = Util_HashString(ss.str());
    ss.write(checksum.hash, 16);

    const string &final = ss.str();
    assert(final.size() == TOTAL_ENTRYSIZE);
    write(fd, final.data(), final.size());
}
