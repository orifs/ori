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

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
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
#include <algorithm>

#include "logging.h"
#include "oripriv.h"

using namespace std;

OriPriv::OriPriv(const std::string &repoPath)
{
    repo = new LocalRepo(repoPath);
    nextId = ORIPRIVID_INVALID + 1;
    nextFH = 1;

    repo->open();

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
    ASSERT(tmpPath[0] != '\0');

    fd = open(tmpPath, O_CREAT|O_RDWR, 0700);
    if (fd < 0)
        throw PosixException(errno);

    return make_pair(tmpPath, fd);
}

/*
 * Current Change Operations
 */

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

    ASSERT(false);

    return NULL;
}

int
OriPriv::closeFH(uint64_t fh)
{
    int status;

    ASSERT(handles.find(fh) != handles.end());
    ASSERT(handles[fh]->fd != -1);

    // Close file
    status = close(handles[fh]->fd);
    handles[fh]->fd = -1;

    // Manage reference count
    handles[fh]->release();
    handles.erase(fh);

    return (status == 0) ? 0 : -errno;
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
    info->fd = -1;

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
    info->path = file.first; // XXX: Change to relative
    info->fd = file.second;

    // Delete any old temporary files
    map<string, OriFileInfo*>::iterator it = paths.find(path);
    if (it != paths.end()) {
        ASSERT(!it->second->isDir());
        if (it->second->type == FILETYPE_TEMPORARY)
            unlink(it->second->path.c_str());
        it->second->release();
    }

    paths[path] = info;
    handles[handle] = info;

    info->retain();

    return make_pair(info, handle);
}

pair<OriFileInfo *, uint64_t>
OriPriv::openFile(const string &path, bool writing, bool trunc)
{
    OriFileInfo *info = getFileInfo(path);
    uint64_t handle = generateFH();

    // XXX: Need to release and remove the hanlde during a failure!

    info->retain();
    handles[handle] = info;

    // Open temporary file if necessary
    if (info->type == FILETYPE_TEMPORARY) {
        int status = open(info->path.c_str(), O_RDWR);
        if (status < 0) {
            ASSERT(false); // XXX: Need to release the handle
            throw PosixException(errno);
        }
        assert(info->fd == -1);
        info->fd = status;
    } else if (info->type == FILETYPE_COMMITTED) {
        ASSERT(!info->hash.isEmpty());
        if (!writing) {
            // Read-only
            info->fd = -1;
        } else if (writing && trunc) {
            // Generate temporary file
            pair<string, int> temp = getTemp();

            info->statInfo.st_size = 0;
            info->statInfo.st_blocks = 0;
            info->type = FILETYPE_TEMPORARY;
            info->path = temp.first;
            info->fd = temp.second;
        } else if (writing) {
            // Copy file
            int status;
            pair<string, int> temp = getTemp();
            close(temp.second);

            if (!repo->copyObject(info->hash, temp.first)) {
                ASSERT(false); // XXX: Need to release the handle
                throw PosixException(EIO);
            }

            status = open(temp.first.c_str(), O_RDWR);
            if (status < 0) {
                ASSERT(false); // XXX: Need to release the handle
                throw PosixException(errno);
            }

            info->type = FILETYPE_TEMPORARY;
            info->path = temp.first;
            info->fd = status;
        } else {
            ASSERT(false);
        }
    } else {
        // XXX: Other types unsupported
        ASSERT(false);
    }

    return make_pair(info, handle);
}

size_t
OriPriv::readFile(OriFileInfo *info, char *buf, size_t size, off_t offset)
{
    ASSERT(info->type == FILETYPE_COMMITTED && !info->hash.isEmpty());

    ObjectType type = repo->getObjectType(info->hash);
    if (type == ObjectInfo::Blob) {
        string payload = repo->getPayload(info->hash);
        // XXX: Cache

        size_t left = payload.size() - offset;
        size_t real_read = min(size, left);

        memcpy(buf, payload.data() + offset, real_read);

        return real_read;
    } else if (type == ObjectInfo::LargeBlob) {
        LargeBlob lb = LargeBlob(repo);
        lb.fromBlob(repo->getPayload(info->hash));
        // XXX: Cache

        size_t total = 0;
        while (total < size) {
            size_t res = lb.read((uint8_t*)(buf + total),
                                 size - total,
                                 offset + total);
            if (res <= 0)
                return res;
            total += res;
        }

        return total;
    }

    return -EIO;
}

void
OriPriv::unlink(const string &path)
{
    OriFileInfo *info = getFileInfo(path);

    ASSERT(info->isSymlink() | info->isReg());

    paths.erase(path);

    // Drop refcount only delete if zero (including temp file)
    info->release();
}

void
OriPriv::rename(const string &fromPath, const string &toPath)
{
    OriFileInfo *info = getFileInfo(fromPath);

    // XXX: Need to rename all files under a directory
    ASSERT(!info->isDir());

    paths.erase(fromPath);
    paths[toPath] = info;

    ASSERT(paths.find(fromPath) == paths.end());
    ASSERT(paths.find(toPath) != paths.end());
}

OriFileInfo *
OriPriv::addDir(const string &path)
{
    OriFileInfo *info = new OriFileInfo();
    time_t now = time(NULL);

    info->statInfo.st_uid = geteuid();
    info->statInfo.st_gid = getegid();
    info->statInfo.st_mode = S_IFDIR;
    info->statInfo.st_nlink = 2;
    info->statInfo.st_size = 512;
    info->statInfo.st_atime = 0;
    info->statInfo.st_mtime = now;
    info->statInfo.st_ctime = now;
    info->type = FILETYPE_TEMPORARY;
    info->id = generateId();
    info->dirLoaded = true;

    dirs[info->id] = new OriDir();
    paths[path] = info;

    return info;
}

void
OriPriv::rmDir(const string &path)
{
    OriFileInfo *info = getFileInfo(path);

    ASSERT(dirs[info->id]->isEmpty());

    dirs.erase(info->id);
    paths.erase(path);

    info->release();
}

OriDir*
OriPriv::getDir(const string &path)
{
    // Check pending directories
    map<string, OriFileInfo*>::iterator it;

    it = paths.find(path);
    if (it != paths.end()) {
        map<OriPrivId, OriDir*>::iterator dit;
        if (!(*it).second->isDir())
            throw PosixException(ENOTDIR);
        if ((*it).second->type == FILETYPE_NULL)
            throw PosixException(ENOENT);
        dit = dirs.find((*it).second->id);
        if (dit == dirs.end())
            goto loadDir;

        return dit->second;
    }

loadDir:
    // Check repository
    ObjectHash hash = repo->lookup(headCommit, path);
    if (!hash.isEmpty()) {
        Tree t = repo->getTree(hash);
        Tree::iterator it;
        OriFileInfo *dirInfo;
        OriDir *dir = new OriDir();

        if (path == "/") {
            dirInfo = new OriFileInfo();
            dirInfo->statInfo.st_uid = geteuid();
            dirInfo->statInfo.st_gid = getegid();
            dirInfo->statInfo.st_mode = 0600 | S_IFDIR;
            dirInfo->statInfo.st_nlink = 2;
            dirInfo->statInfo.st_blksize = 4096;
            dirInfo->statInfo.st_blocks = 1;
            dirInfo->statInfo.st_size = 512;
            dirInfo->statInfo.st_mtime = headCommit.getTime();
            dirInfo->statInfo.st_ctime = headCommit.getTime();
            dirInfo->type = FILETYPE_COMMITTED;
            dirInfo->id = generateId();

            paths["/"] = dirInfo;
        } else {
            dirInfo = getFileInfo(path);
        }

        for (it = t.begin(); it != t.end(); it++) {
            OriFileInfo *info = new OriFileInfo();
            AttrMap *attrs = &it->second.attrs;
            struct passwd *pw = getpwnam(attrs->getAsStr(ATTR_USERNAME).c_str());

            if (it->second.type == TreeEntry::Tree) {
                info->statInfo.st_mode = S_IFDIR;
                info->statInfo.st_nlink = 2;
                // XXX: This is hacky but a directory gets the correct nlink 
                // value once it is opened for the first time.
                dirInfo->statInfo.st_nlink++;
            } else {
                info->statInfo.st_mode = S_IFREG;
                info->statInfo.st_nlink = 1;
            }
            info->statInfo.st_mode |= attrs->getAs<mode_t>(ATTR_PERMS);
            info->statInfo.st_uid = pw->pw_uid;
            info->statInfo.st_gid = pw->pw_gid;
            info->statInfo.st_size = attrs->getAs<size_t>(ATTR_FILESIZE);
            info->statInfo.st_blocks = info->statInfo.st_size / 512;
            info->statInfo.st_mtime = attrs->getAs<time_t>(ATTR_MTIME);
            info->statInfo.st_ctime = attrs->getAs<time_t>(ATTR_CTIME);
            info->type = FILETYPE_COMMITTED;
            info->id = generateId();
            info->hash = it->second.hash;

            dir->add(it->first, info->id);
            if (path == "/")
                paths["/" + it->first] = info;
            else
                paths[path + "/" + it->first] = info;
        }

        dirInfo->dirLoaded = true;
        dirs[dirInfo->id] = dir;
        return dir;
    }

    // Check root
    if (path == "/" && head.isEmpty()) {
        time_t now = time(NULL);
        OriFileInfo *info = new OriFileInfo();

        info->statInfo.st_uid = geteuid();
        info->statInfo.st_gid = getegid();
        info->statInfo.st_mode = 0600 | S_IFDIR;
        info->statInfo.st_nlink = 2;
        info->statInfo.st_blksize = 4096;
        info->statInfo.st_blocks = 1;
        info->statInfo.st_size = 512;
        info->statInfo.st_mtime = now;
        info->statInfo.st_ctime = now;
        info->type = FILETYPE_TEMPORARY;
        info->id = generateId();
        info->dirLoaded = true;

        dirs[info->id] = new OriDir();
        paths["/"] = info;

        return dirs[info->id];
    }

    throw PosixException(ENOENT);
}

void
OriPrivCheckDir(OriPriv *priv, const string &path, OriDir *dir)
{
    OriDir::iterator it;

    for (it = dir->begin(); it != dir->end(); it++) {
        string objPath = path + "/" + it->first;
        OriFileInfo *info = NULL;

        try {
            info = priv->getFileInfo(objPath);
        } catch (PosixException e) {
            FUSE_LOG("fsck: getFileInfo(%s) had %s",
                     objPath.c_str(), e.what());
        }

        if (info && info->isDir() && info->dirLoaded) {
            OriDir *dir;

            ASSERT(!info->isSymlink() && !info->isReg());

            try {
                dir = priv->getDir(objPath);
                OriPrivCheckDir(priv, objPath, dir);
            } catch (PosixException e) {
                FUSE_LOG("fsck: getDir(%s) encountered %s",
                         objPath.c_str(), e.what());
            }
        }

        if (info && info->isReg()) {
            if (info->isDir()) {
                FUSE_LOG("fsck: %s is marked as %s%s%s",
                         objPath.c_str(),
                         info->isSymlink() ? "Sym" : "",
                         info->isReg() ? "Reg" : "",
                         info->isDir() ? "Dir" : "");
            }
        }

        if (info && info->isSymlink()) {
            if (!info->isReg() || info->isDir()) {
                FUSE_LOG("fsck: %s is marked as %s%s%s",
                         objPath.c_str(),
                         info->isSymlink() ? "Sym" : "",
                         info->isReg() ? "Reg" : "",
                         info->isDir() ? "Dir" : "");
            }
        }

        if (info && info->id != it->second) {
            FUSE_LOG("fsck: %s object Id mismatch!", objPath.c_str());
        }
    }
}

/*
 * Snapshot Operations
 */

map<string, ObjectHash>
OriPriv::listSnapshots()
{
    return repo->listSnapshots();
}

Commit
OriPriv::lookupSnapshot(const string &name)
{
    ObjectHash hash = repo->lookupSnapshot(name);

    return repo->getCommit(hash);
}

Tree
OriPriv::getTree(const Commit &c, const string &path)
{
    ObjectHash hash = repo->lookup(c, path);

    return repo->getTree(hash);
}

/*
 * Debugging
 */

void
OriPriv::fsck()
{
    map<string, OriFileInfo *>::iterator it;
    OriDir *dir = getDir("/");

    OriPrivCheckDir(this, "", dir);

    for (it = paths.begin(); it != paths.end(); it++) {
        string basename = StrUtil_Basename(it->first);
        string parentPath = StrUtil_Dirname(it->first);
        OriDir *dir = NULL;

        if (it->first == "/")
            continue;

        if (parentPath == "")
            parentPath = "/";

        try {
            dir = getDir(parentPath);
        } catch (PosixException e) {
            FUSE_LOG("fsck: %s path encountered an error %s",
                     it->first.c_str(), e.what());
        }

        if (dir) {
            OriDir::iterator dirIt = dir->find(basename);

            if (dirIt == dir->end()) {
                FUSE_LOG("fsck: %s not present in directory!",
                         it->first.c_str());
            } else if (dirIt->second != it->second->id) {
                FUSE_LOG("fsck: %s object Id mismatch!", it->first.c_str());
            }
        }
    }
}

OriPriv *
GetOriPriv()
{
    return (OriPriv*)fuse_get_context()->private_data;
}

