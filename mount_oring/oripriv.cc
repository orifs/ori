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
#include <ori/rwlock.h>
#include <ori/objecthash.h>
#include <ori/commit.h>
#include <ori/localrepo.h>

#include <string>
#include <map>

#include "logging.h"
#include "oripriv.h"

using namespace std;

OriPriv::OriPriv(const std::string &repoPath)
{
    repo = new LocalRepo(repoPath);
    nextId = ORIPRIVID_INVALID + 1;
    nextFH = 1;

    RWLock::LockOrderVector order;
    order.push_back(ioLock.lockNum);
    order.push_back(nsLock.lockNum);
    order.push_back(cmdLock.lockNum);
    RWLock::setLockOrder(order);

    reset();
}

OriPriv::~OriPriv()
{
}

void
OriPriv::reset()
{
    head = repo->getHead();
    if (!head.isEmpty()) {
        headCommit = repo->getCommit(head);
    }

    // Create temporary directory
    tmpDir = repo->getRootPath() + ORI_PATH_TMP + "fuse";
    if (::mkdir(tmpDir.c_str(), 0700) < 0)
        throw PosixException(errno);
}

pair<string, int>
OriPriv::getTemp()
{
    string filePath = tmpDir + "/obj.XXXXXX";
    char tmpPath[PATH_MAX];
    int fd;

    strncpy(tmpPath, filePath.c_str(), PATH_MAX);

    mktemp(tmpPath);
    assert(tmpPath[0] != '\0');

    fd = open(tmpPath, O_CREAT|O_RDWR, 0700);
    if (fd < 0)
        throw PosixException(errno);

    return make_pair(tmpPath, fd);
}

uint64_t
OriPriv::generateFH()
{
    uint64_t fh = nextFH;

    nextFH++;

    return fh;
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
        OriFileInfo *info = (*it).second;

        if (info->type == FILETYPE_NULL)
            throw PosixException(ENOENT);

        return info;
    }

    // Check repository

    throw PosixException(ENOENT);
}

OriFileInfo *
OriPriv::getFileInfo(uint64_t fh)
{
    map<uint64_t, OriFileInfo*>::iterator it;

    it = handles.find(fh);
    if (it != handles.end()) {
        return (*it).second;
    }

    assert(false);

    return NULL;
}

void
OriPriv::closeFH(uint64_t fh)
{
    // Manage reference count
    handles[fh]->release();

    handles.erase(fh);
}

OriFileInfo *
OriPriv::createInfo()
{
    OriFileInfo *info = new OriFileInfo();
    time_t now = time(NULL);

    // XXX: Switch to using fuse_context
    info->statInfo.st_uid = geteuid();
    info->statInfo.st_gid = getegid();
    info->statInfo.st_blksize = 4096;
    info->statInfo.st_nlink = 1;
    info->statInfo.st_atime = 0;
    info->statInfo.st_mtime = now; // XXX: NOW
    info->statInfo.st_ctime = now;
    info->type = FILETYPE_TEMPORARY;
    info->id = generateId();

    return info;
}

OriFileInfo *
OriPriv::addSymlink(const string &path)
{
    OriFileInfo *info = createInfo();

    info->statInfo.st_mode = S_IFLNK;
    // XXX: Adjust size properly

    paths[path] = info;

    return info;
}

pair<OriFileInfo *, uint64_t>
OriPriv::addFile(const string &path)
{
    OriFileInfo *info = createInfo();
    pair<string, int> file = getTemp();
    uint64_t handle = generateFH();

    info->statInfo.st_mode = S_IFREG;
    // XXX: Adjust size properly
    info->statInfo.st_size = 0;
    info->link = file.first; // XXX: Change to relative
    info->fd = file.second;

    paths[path] = info;
    handles[handle] = info;

    info->retain();

    return make_pair(info, handle);
}

pair<OriFileInfo *, uint64_t>
OriPriv::openFile(const string &path)
{
    OriFileInfo *info = getFileInfo(path);
    uint64_t handle = generateFH();

    info->retain();
    handles[handle] = info;

    // XXX: Open temporary file if necessary

    return make_pair(info, handle);
}

void
OriPriv::unlink(const string &path)
{
    OriFileInfo *info = getFileInfo(path);

    assert(info->isSymlink() | info->isReg());

    paths.erase(path);

    // Drop refcount only delete if zero (including temp file)
    info->release();
}

void
OriPriv::rename(const string &fromPath, const string &toPath)
{
    OriFileInfo *info = getFileInfo(fromPath);

    paths.erase(fromPath);
    paths[toPath] = info;
}

OriFileInfo *
OriPriv::addDir(const string &path)
{
    OriFileInfo *info = new OriFileInfo();
    time_t now = time(NULL);

    info->statInfo.st_uid = geteuid();
    info->statInfo.st_gid = getegid();
    info->statInfo.st_mode = S_IFDIR;
    // XXX: Adjust nlink and size properly
    info->statInfo.st_nlink = 1;
    info->statInfo.st_size = 1;
    info->statInfo.st_atime = 0;
    info->statInfo.st_mtime = now; // XXX: NOW
    info->statInfo.st_ctime = now;
    info->type = FILETYPE_TEMPORARY;
    info->id = generateId();

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

    info->release();
}

OriDir&
OriPriv::getDir(const string &path)
{
    // Check pending directories
    map<string, OriFileInfo*>::iterator it;

    it = paths.find(path);
    if (it != paths.end()) {
        if (!(*it).second->isDir())
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
        info->statInfo.st_blksize = 4096;
        info->statInfo.st_blocks = 1;
        info->statInfo.st_size = 512;
        info->statInfo.st_mtime = 0; // Use latest commit time
        info->statInfo.st_ctime = 0;
        info->type = FILETYPE_TEMPORARY;
        info->id = generateId();

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

