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
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include <ori/posixexception.h>
#include <ori/objecthash.h>

#include <string>
#include <map>

#include "logging.h"
#include "oripriv.h"

using namespace std;

OriPriv::OriPriv()
{
    nextId = ORIPRIVID_INVALID + 1;
}

OriPriv::~OriPriv()
{
}

OriPrivId
OriPriv::generateId()
{
    OriPrivId id = nextId;

    nextId++;

    return id;
}

OriFileInfo *
OriPriv::getFileInfo(const string &path)
{
    map<string, OriFileInfo*>::iterator it;

    // Check pending directories
    it = paths.find(path);
    if (it != paths.end()) {
        if (!(*it).second->isDir)
            throw PosixException(ENOTDIR);
        if ((*it).second->type == FILETYPE_NULL)
            throw PosixException(ENOENT);

        return (*it).second;
    }

    // Check repository

    throw PosixException(ENOENT);
}

OriFileInfo *
OriPriv::addDir(const string &path)
{
    OriFileInfo *info = new OriFileInfo();

    info->statInfo.st_uid = geteuid();
    info->statInfo.st_gid = getegid();
    info->statInfo.st_mode = S_IFDIR;
    // XXX: Adjust nlink and size properly
    info->statInfo.st_nlink = 1;
    info->statInfo.st_size = 1;
    info->statInfo.st_mtime = 0; // XXX: NOW
    info->statInfo.st_ctime = 0;
    info->type = FILETYPE_TEMPORARY;
    info->id = generateId();
    info->isDir = true;

    dirs[info->id] = new OriDir();
    paths[path] = info;

    return info;
}

void
OriPriv::rmDir(const string &path)
{
    OriFileInfo *info = getFileInfo(path);

    assert(dirs[info->id]->isEmpty());

    dirs.erase(info->id);
    paths.erase(path);

    delete info;
}

OriDir&
OriPriv::getDir(const string &path)
{
    // Check pending directories
    map<string, OriFileInfo*>::iterator it;

    it = paths.find(path);
    if (it != paths.end()) {
        if (!(*it).second->isDir)
            throw PosixException(ENOTDIR);
        if ((*it).second->type == FILETYPE_NULL)
            throw PosixException(ENOENT);

        return *dirs[(*it).second->id];
    }

    // Check repository

    // Check root
    if (path == "/") {
        OriFileInfo *info = new OriFileInfo();

        info->statInfo.st_uid = geteuid();
        info->statInfo.st_gid = getegid();
        info->statInfo.st_mode = 0600 | S_IFDIR;
        // Adjust nlink and size properly
        info->statInfo.st_nlink = 1;
        info->statInfo.st_size = 512;
        info->statInfo.st_mtime = 0; // Use latest commit time
        info->statInfo.st_ctime = 0;
        info->type = FILETYPE_TEMPORARY;
        info->id = generateId();
        info->isDir = true;

        dirs[info->id] = new OriDir();
        paths["/"] = info;

        return *dirs[info->id];
    }

    throw PosixException(ENOENT);
}

OriPriv *
GetOriPriv()
{
    return (OriPriv*)fuse_get_context()->private_data;
}

