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

#include <fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <map>
#include <iostream>

#include "debug.h"
#include "object.h"
#include "index.h"

using namespace std;

/// Entry consist of the hash and the serialized object info.
#define INDEX_ENTRYSIZE (64 + 16)

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

    if (sb.st_size % INDEX_ENTRYSIZE != 0) {
        cout << "Index seems dirty please rebuild it!" << endl;
        ::close(fd);
        fd = -1;
        exit(1);
        return;
    }

    entries = sb.st_size / INDEX_ENTRYSIZE;
    for (i = 0; i < entries; i++) {
        int status;
        char entry[INDEX_ENTRYSIZE];
        string hash, info;
        ObjectInfo *objInfo;

        status = read(fd, &entry, INDEX_ENTRYSIZE);
        assert(status == INDEX_ENTRYSIZE);

        hash.assign(entry, 64);
        info.assign(entry + 64, 16);

        objInfo = new ObjectInfo(hash.c_str());
        objInfo->setInfo(info);

        index[hash] = objInfo;
    }
    ::close(fd);

    // Reopen append only
    fd = ::open(indexFile.c_str(), O_WRONLY | O_APPEND);
    assert(fd >= 0); // Assume that the repository lock protects the index
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
Index::dump()
{
    map<string, ObjectInfo *>::iterator it;

    cout << "***** BEGIN REPOSITORY INDEX *****" << endl;
    for (it = index.begin(); it != index.end(); it++)
    {
        cout << (*it).first << endl;
    }
    cout << "***** END REPOSITORY INDEX *****" << endl;
}

void
Index::updateInfo(const string &objId, const ObjectInfo &info)
{
    string indexLine;

    indexLine = objId;
    indexLine += info.getInfo();

    write(fd, indexLine.data(), indexLine.size());
}

ObjectInfo *
Index::getInfo(const string &objId)
{
    map<string, ObjectInfo *>::iterator it;

    it = index.find(objId);

    if (it != index.end())
        return (*it).second;
    else
        return NULL;
}

bool
Index::hasObject(const string &objId)
{
    map<string, ObjectInfo *>::iterator it;

    it = index.find(objId);

    return it != index.end();
}

map<string, ObjectInfo *>
Index::getList()
{
    return index;
}

