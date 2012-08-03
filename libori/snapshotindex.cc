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

#include <fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <sstream>
#include <iostream>
#include <map>
#include <set>

#include "debug.h"
#include "util.h"
#include "snapshotindex.h"

using namespace std;

SnapshotIndex::SnapshotIndex()
{
    fd = -1;
}

SnapshotIndex::~SnapshotIndex()
{
    close();
}

void
SnapshotIndex::open(const string &indexFile)
{
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
    };


    if (::fstat(fd, &sb) < 0) {
        perror("fstat");
        return;
    }

    int len = sb.st_size;
    char *buf = new char[len];

    int status = read(fd, buf, len);
    assert(status == len);

    string blob = string().assign(buf, len);
    string line;
    stringstream ss(blob);

    while (getline(ss, line, '\n')) {
	ObjectHash hash;
        string name;

        hash = ObjectHash::fromHex(line.substr(0, 64));
        name = line.substr(65);

        snapshots[name] = hash;
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
SnapshotIndex::close()
{
    if (fd != -1) {
        ::fsync(fd);
        ::close(fd);
        fd = -1;
    }
}

void
SnapshotIndex::rewrite()
{
    int fdNew, tmpFd;
    string newIndex = fileName + ".tmp";
    map<string, ObjectHash>::iterator it;

    fdNew = ::open(newIndex.c_str(), O_RDWR | O_CREAT,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fdNew < 0) {
        perror("open");
        cout << "Could not open a temporary index file!" << endl;
        return;
    };

    // Write new index
    for (it = snapshots.begin(); it != snapshots.end(); it++)
    {
        int status;
        string indexLine;

        indexLine = (*it).second.hex() + " " + (*it).first + "\n";

        status = write(fdNew, indexLine.data(), indexLine.size());
        assert(status == (int)indexLine.size());
    }

    Util_RenameFile(newIndex, fileName);

    tmpFd = fd;
    fd = fdNew;
    ::close(tmpFd);
}

void
SnapshotIndex::addSnapshot(const string &name, const ObjectHash &commitId)
{
    string indexLine;

    assert(!commitId.isEmpty());

    indexLine = commitId.hex() + " " + name + "\n";

    write(fd, indexLine.data(), indexLine.size());
}

void
SnapshotIndex::delSnapshot(const std::string &name)
{
    // delete in map

    rewrite();
}

const ObjectHash &
SnapshotIndex::getSnapshot(const string &name) const
{
    map<string, ObjectHash>::const_iterator it = snapshots.find(name);
    assert(it != snapshots.end());

    return (*it).second;
}

map<string, ObjectHash>
SnapshotIndex::getList()
{
    return snapshots;
}

