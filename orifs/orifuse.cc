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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>

#include <getopt.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include <string>
#include <map>
#include <boost/tr1/memory.hpp>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/orifile.h>
#include <oriutil/systemexception.h>
#include <oriutil/rwlock.h>
#include <ori/repostore.h>
#include <ori/version.h>
#include <ori/commit.h>
#include <ori/localrepo.h>
#include <ori/remoterepo.h>

#include "logging.h"
#include "oricmd.h"
#include "oripriv.h"
#include "oriopt.h"

#ifdef DEBUG
#define FSCK_A_LOT
#endif

using namespace std;

#define ORI_CONTROL_FILENAME ".ori_control"
#define ORI_CONTROL_FILEPATH "/" ORI_CONTROL_FILENAME
#define ORI_SNAPSHOT_DIRNAME ".snapshot"
#define ORI_SNAPSHOT_DIRPATH "/" ORI_SNAPSHOT_DIRNAME

mount_ori_config config;
RemoteRepo remoteRepo;
OriPriv *priv;

// Mount/Unmount

static void *
ori_init(struct fuse_conn_info *conn)
{
    FUSE_LOG("Ori Filesystem starting ...");

    return priv;
}

static void
ori_destroy(void *userdata)
{
    OriPriv *priv = GetOriPriv();
    priv->commit("FUSE commit on unmount");
    priv->cleanup();
    delete priv;

    FUSE_LOG("File system unmounted");
}

// File Manipulation

static int
ori_mknod(const char *path, mode_t mode, dev_t dev)
{
    return -EPERM;
}

static int
ori_unlink(const char *path)
{
    OriPriv *priv = GetOriPriv();
    string parentPath;

#ifdef FSCK_A_LOT
    priv->fsck();
#endif /* FSCK_A_LOT */

    FUSE_LOG("FUSE ori_unlink(path=\"%s\")", path);

    parentPath = OriFile_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        return -EACCES;
    } else if (strncmp(path,
                ORI_SNAPSHOT_DIRPATH,
                strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        return -EACCES;
    }

    try {
        OriDir *parentDir = priv->getDir(parentPath);
        OriFileInfo *info = priv->getFileInfo(path);

        if (info->isDir())
            return -EPERM;

        parentDir->remove(OriFile_Basename(path));
        if (info->isSymlink() || info->isReg()) {
            if (info->type == FILETYPE_TEMPORARY)
                unlink(info->path.c_str());

            priv->unlink(path);
        } else {
            // XXX: Support files
            ASSERT(false);
        }
    } catch (SystemException e) {
        return -e.getErrno();
    }

    priv->journal("unlink", path);

    return 0;
}

static int
ori_symlink(const char *target_path, const char *link_path)
{
    OriPriv *priv = GetOriPriv();
    OriDir *parentDir;
    string parentPath;

#ifdef FSCK_A_LOT
    priv->fsck();
#endif /* FSCK_A_LOT */

    FUSE_LOG("FUSE ori_symlink(path=\"%s\")", link_path);

    parentPath = OriFile_Dirname(link_path);
    if (parentPath == "")
        parentPath = "/";

    if (strcmp(link_path, ORI_CONTROL_FILEPATH) == 0) {
        return -EACCES;
    } else if (strncmp(link_path,
                ORI_SNAPSHOT_DIRPATH,
                strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        return -EACCES;
    }

    try {
        parentDir = priv->getDir(parentPath);
    } catch (SystemException e) {
        return -e.getErrno();
    }

    OriFileInfo *info = priv->addSymlink(link_path);
    info->statInfo.st_mode |= 0755;
    info->path = target_path;
    info->statInfo.st_size = info->path.length();

    parentDir->add(OriFile_Basename(link_path), info->id);

    return 0;
}

static int
ori_readlink(const char *path, char *buf, size_t size)
{
    OriPriv *priv = GetOriPriv();
    OriFileInfo *info;

#ifdef FSCK_A_LOT
    priv->fsck();
#endif /* FSCK_A_LOT */

    FUSE_LOG("FUSE ori_readlink(path\"%s\", size=%ld)", path, size);

    try {
        info = priv->getFileInfo(path);
    } catch (SystemException e) {
        return -e.getErrno();
    }

    memcpy(buf, info->path.c_str(), MIN(info->path.length() + 1, size));

    return 0;
}

static int
ori_rename(const char *from_path, const char *to_path)
{
    OriPriv *priv = GetOriPriv();
    string fromParent, toParent;

#ifdef FSCK_A_LOT
    priv->fsck();
#endif /* FSCK_A_LOT */

    FUSE_LOG("FUSE ori_rename(from_path=\"%s\", to_path=\"%s\")",
             from_path, to_path);

    fromParent = OriFile_Dirname(from_path);
    if (fromParent == "")
        fromParent = "/";
    toParent = OriFile_Dirname(to_path);
    if (toParent == "")
        toParent = "/";

    if (strncmp(to_path,
                ORI_SNAPSHOT_DIRPATH,
                strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        return -EACCES;
    }
    if (strncmp(from_path,
                ORI_SNAPSHOT_DIRPATH,
                strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        return -EACCES;
    }

    try {
        OriDir *fromDir = priv->getDir(fromParent);
        OriDir *toDir = priv->getDir(toParent);
        OriFileInfo *info = priv->getFileInfo(from_path);
        OriFileInfo *toFile = NULL;
        OriDir *toFileDir = NULL;

        try {
            toFile = priv->getFileInfo(to_path);
        } catch (SystemException e) {
            // Fall through
        }

        // Not sure if FUSE checks for these two error cases
        if (toFile != NULL && toFile->isDir()) {
            toFileDir = priv->getDir(to_path);

            if (!toFileDir->isEmpty())
                return -ENOTEMPTY;
        }
        if (toFile != NULL && info->isDir() && !toFile->isDir()) {
            return -EISDIR;
        }

        // XXX: Need to support renaming directories (nlink, OriPriv::Rename)
        if (info->isDir()) {
            FUSE_LOG("ori_rename: Directory rename attempted %s to %s",
                     from_path, to_path);
            return -EINVAL;
        }

        priv->rename(from_path, to_path);

        string from = OriFile_Basename(from_path);
        string to = OriFile_Basename(to_path);
        FUSE_LOG("%s %s", from.c_str(), to.c_str());

        fromDir->remove(from);
        toDir->add(to, info->id);

        // Delete previously present file
        if (toFile != NULL) {
            toFile->release();
        }
    } catch (SystemException e) {
        return -e.getErrno();
    }

    string journalArg = from_path;
    journalArg += ":";
    journalArg += to_path;
    priv->journal("rename", journalArg);

    return 0;
}

// File IO

static int
ori_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    OriPriv *priv = GetOriPriv();
    string parentPath;
    OriDir *parentDir;

#ifdef FSCK_A_LOT
    priv->fsck();
#endif /* FSCK_A_LOT */

    FUSE_LOG("FUSE ori_create(path=\"%s\")", path);

    parentPath = OriFile_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    if (strncmp(path,
                ORI_SNAPSHOT_DIRPATH,
                strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        return -EACCES;
    }

    try {
        parentDir = priv->getDir(parentPath);
    } catch (SystemException e) {
        return -e.getErrno();
    }

    pair<OriFileInfo *, uint64_t> info = priv->addFile(path);
    info.first->statInfo.st_mode |= mode;

    parentDir->add(OriFile_Basename(path), info.first->id);

    string journalArg = path;
    journalArg += ":" + info.first->path;
    priv->journal("create", journalArg);

    // Set fh
    fi->fh = info.second;

    return 0;
}

static int
ori_open(const char *path, struct fuse_file_info *fi)
{
    OriPriv *priv = GetOriPriv();
    string parentPath;
    OriDir *parentDir;
    pair<OriFileInfo *, uint64_t> info;
    bool writing = false;
    bool trunc = false;
    
    if (fi->flags & O_WRONLY || fi->flags & O_RDWR)
        writing = true;
    if (fi->flags & O_TRUNC)
        trunc = true;

    FUSE_LOG("FUSE ori_open(path=\"%s\")", path);

    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        return 0;
    } else if (strncmp(path,
                       ORI_SNAPSHOT_DIRPATH,
                       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        return writing ? -EPERM : 0;
    }

    parentPath = OriFile_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    try {
        parentDir = priv->getDir(parentPath);
        info = priv->openFile(path, /*writing*/writing, /*trunc*/trunc);
    } catch (SystemException e) {
        return -e.getErrno();
    }

    if (writing)
        parentDir->setDirty();

    // Set fh
    fi->fh = info.second;

    return 0;
}

static int
ori_read(const char *path, char *buf, size_t size, off_t offset,
         struct fuse_file_info *fi)
{
    OriPriv *priv = GetOriPriv();
    OriFileInfo *info;
    int status;

    // FUSE_LOG("FUSE ori_read(path=\"%s\", length=%ld, offset=%ld)",
    //          path, size, offset);

    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        return priv->cmd.read(buf, size, offset);
    } else if (strncmp(path,
                       ORI_SNAPSHOT_DIRPATH,
                       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        string snapshot = path;
        string parentPath, fileName;
        size_t pos = 0;
        Commit c;
        Tree t;
        
        snapshot = snapshot.substr(strlen(ORI_SNAPSHOT_DIRPATH) + 1);
        pos = snapshot.find('/', pos);

        ASSERT(pos != snapshot.npos);

        parentPath = snapshot.substr(pos);
        snapshot = snapshot.substr(0, pos);
        fileName = OriFile_Basename(parentPath);
        parentPath = OriFile_Dirname(parentPath);
        if (parentPath == "")
            parentPath = "/";

        // XXX: Enforce that this is a valid snapshot & directory path
        c = priv->lookupSnapshot(snapshot);
        t = priv->getTree(c, parentPath);

        // lookup tree
        Tree::iterator it = t.find(fileName);
        if (it == t.end())
            return -ENOENT;

        // Read
        OriFileInfo *tempInfo = new OriFileInfo();
        tempInfo->type = FILETYPE_COMMITTED;
        tempInfo->hash = it->second.hash;
        status = priv->readFile(tempInfo, buf, size, offset);
        tempInfo->release();
        return status;
    }

    info = priv->getFileInfo(fi->fh);
    if (info->fd != -1) {
        // File in temporary directory
        status = pread(info->fd, buf, size, offset);
        if (status < 0)
            return -errno;
    } else {
        // File in repository
        return priv->readFile(info, buf, size, offset);
    }

    return status;
}

static int
ori_write(const char *path, const char *buf, size_t size, off_t offset,
         struct fuse_file_info *fi)
{
    OriPriv *priv = GetOriPriv();
    OriFileInfo *info;
    int status;

    // FUSE_LOG("FUSE ori_write(path=\"%s\", length=%ld)", path, size);

    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        return priv->cmd.write(buf, size, offset);
    } else if (strncmp(path,
                       ORI_SNAPSHOT_DIRPATH,
                       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        ASSERT(false);
        return -EIO;
    }

    info = priv->getFileInfo(fi->fh);
    status = pwrite(info->fd, buf, size, offset);
    if (status < 0)
        return -errno;

    // Update size
    if (info->statInfo.st_size < (off_t)size + offset) {
        info->statInfo.st_size = size + offset;
        info->statInfo.st_blocks = (size + offset + (512-1))/512;
    }

    return status;
}

static int
ori_truncate(const char *path, off_t length)
{
    OriPriv *priv = GetOriPriv();
    OriFileInfo *info;

    FUSE_LOG("FUSE ori_truncate(path=\"%s\", length=%ld)", path, length);

    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        // XXX: Not implemented
        return 0;
    } else if (strncmp(path,
                       ORI_SNAPSHOT_DIRPATH,
                       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        return -EACCES;
    }

    info = priv->getFileInfo(path);
    if (info->type == FILETYPE_TEMPORARY) {
        int status;

        status = truncate(info->path.c_str(), length);
        if (status < 0)
            return -errno;

        // Update size
        info->statInfo.st_size = length;
        info->statInfo.st_blocks = (length + (512-1))/512;

        return status;
    } else {
        // XXX: Not Implemented
        ASSERT(false);
        return -EINVAL;
    }
}

static int
ori_ftruncate(const char *path, off_t length, struct fuse_file_info *fi)
{
    OriPriv *priv = GetOriPriv();
    OriFileInfo *info;

    FUSE_LOG("FUSE ori_ftruncate(path=\"%s\", length=%ld)", path, length);

    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        // XXX: Not implemented
        return 0;
    } else if (strncmp(path,
                       ORI_SNAPSHOT_DIRPATH,
                       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        ASSERT(false);
        return -EIO;
    }

    info = priv->getFileInfo(fi->fh);
    if (info->type == FILETYPE_TEMPORARY) {
        int status;

        status = ftruncate(info->fd, length);
        if (status < 0)
            return -errno;

        // Update size
        info->statInfo.st_size = length;
        info->statInfo.st_blocks = (length + (512-1))/512;

        return status;
    } else {
        // XXX: Not Implemented
        ASSERT(false);
        return -EINVAL;
    }
}

static int
ori_release(const char *path, struct fuse_file_info *fi)
{
    OriPriv *priv = GetOriPriv();

    FUSE_LOG("FUSE ori_release(path=\"%s\"): fh=%ld", path, fi->fh);

    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        return 0;
    } else if (strncmp(path,
                       ORI_SNAPSHOT_DIRPATH,
                       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        return 0;
    }

    // Decrement reference count (deletes temporary file for unlink)
    return priv->closeFH(fi->fh);
}

// Directory Operations

static int
ori_mkdir(const char *path, mode_t mode)
{
    OriPriv *priv = GetOriPriv();
    OriDir *parentDir;
    OriFileInfo *parentInfo;
    string parentPath;

#ifdef FSCK_A_LOT
    priv->fsck();
#endif /* FSCK_A_LOT */

    FUSE_LOG("FUSE ori_mkdir(path=\"%s\")", path);

    parentPath = OriFile_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    if (strncmp(path,
                ORI_SNAPSHOT_DIRPATH,
                strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        return -EACCES;
    }

    try {
        parentDir = priv->getDir(parentPath);
        parentInfo = priv->getFileInfo(parentPath);
    } catch (SystemException e) {
        return -e.getErrno();
    }

    OriFileInfo *info = priv->addDir(path);
    info->statInfo.st_mode |= mode;

    parentDir->add(OriFile_Basename(path), info->id);
    parentInfo->statInfo.st_nlink++;

    priv->journal("mkdir", path);

    return 0;
}

static int
ori_rmdir(const char *path)
{
    OriPriv *priv = GetOriPriv();
    string parentPath;

#ifdef FSCK_A_LOT
    priv->fsck();
#endif /* FSCK_A_LOT */

    FUSE_LOG("FUSE ori_rmdir(path=\"%s\")", path);

    parentPath = OriFile_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    if (strncmp(path,
                ORI_SNAPSHOT_DIRPATH,
                strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        return -EACCES;
    }

    try {
        OriDir *parentDir = priv->getDir(parentPath);
        OriFileInfo *parentInfo = priv->getFileInfo(parentPath);
        OriDir *dir = priv->getDir(path);

        if (!dir->isEmpty()) {
            OriDir::iterator it;

            FUSE_LOG("Directory not empty!");
            for (it = dir->begin(); it != dir->end(); it++) {
                FUSE_LOG("DIR: %s\n", it->first.c_str());
            }

            return -ENOTEMPTY;
        }

        parentDir->remove(OriFile_Basename(path));
        parentInfo->statInfo.st_nlink--;
        priv->rmDir(path);

        ASSERT(parentInfo->statInfo.st_nlink >= 2);
    } catch (SystemException e) {
        return -e.getErrno();
    }

    priv->journal("rmdir", path);

    return 0;
}

static int
ori_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi)
{
    OriPriv *priv = GetOriPriv();
    OriDir *dir;
    OriDir::iterator it;
    string dirPath = path;

    if (dirPath != "/")
        dirPath += "/";

#ifdef FSCK_A_LOT
    priv->fsck();
#endif /* FSCK_A_LOT */

    FUSE_LOG("FUSE ori_readdir(path=\"%s\", offset=%ld)", path, offset);

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    if (strcmp(path, "/") == 0) {
        filler(buf, ORI_CONTROL_FILENAME, NULL, 0);
        filler(buf, ORI_SNAPSHOT_DIRNAME, NULL, 0);
    } else if (strcmp(path, ORI_SNAPSHOT_DIRPATH) == 0) {
        map<string, ObjectHash> snapshots = priv->listSnapshots();
        map<string, ObjectHash>::iterator it;

        for (it = snapshots.begin(); it != snapshots.end(); it++) {
            filler(buf, (*it).first.c_str(), NULL, 0);
        }

        return 0;
    } else if (strncmp(path,
                       ORI_SNAPSHOT_DIRPATH,
                       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        string snapshot = path;
        string relPath;
        size_t pos = 0;
        Commit c;
        Tree t;
        
        snapshot = snapshot.substr(strlen(ORI_SNAPSHOT_DIRPATH) + 1);
        pos = snapshot.find('/', pos);

        if (pos == snapshot.npos) {
            relPath = "/";
        } else {
            relPath = snapshot.substr(pos);
            snapshot = snapshot.substr(0, pos);
        }

        // XXX: Enforce that this is a valid snapshot & directory path
        c = priv->lookupSnapshot(snapshot);
        t = priv->getTree(c, relPath);

        for (map<string, TreeEntry>::iterator it = t.tree.begin();
             it != t.tree.end();
             it++) {
            filler(buf, (*it).first.c_str(), NULL, 0);
        }

        return 0;
    }

    try {
        dir = priv->getDir(path);
    } catch (SystemException e) {
        return -e.getErrno();
    }

    for (it = dir->begin(); it != dir->end(); it++) {
        OriFileInfo *info;
        
        try {
            info = priv->getFileInfo(dirPath + (*it).first);
            filler(buf, (*it).first.c_str(), &info->statInfo, 0);
        } catch (SystemException e) {
            FUSE_LOG("Unexpected %s", e.what());
            filler(buf, (*it).first.c_str(), NULL, 0);
        }
    }

    return 0;
}


// File Attributes

static int
ori_getattr(const char *path, struct stat *stbuf)
{
    OriPriv *priv = GetOriPriv();

    FUSE_LOG("FUSE ori_getattr(path=\"%s\")", path);

    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        stbuf->st_uid = geteuid();
        stbuf->st_gid = getegid();
        stbuf->st_mode = 0600 | S_IFREG;
        stbuf->st_nlink = 1;
        stbuf->st_size = priv->cmd.readSize();
        stbuf->st_blksize = 4096;
        stbuf->st_blocks = (stbuf->st_size + 511) / 512;
        return 0;
    } else if (strcmp(path, ORI_SNAPSHOT_DIRPATH) == 0) {
        stbuf->st_uid = geteuid();
        stbuf->st_gid = getegid();
        stbuf->st_mode = 0600 | S_IFDIR;
        stbuf->st_nlink = 2;
        stbuf->st_size = 512;
        stbuf->st_blksize = 4096;
        stbuf->st_blocks = 1;
        return 0;
    } else if (strncmp(path,
                       ORI_SNAPSHOT_DIRPATH,
                       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        string snapshot = path;
        string parentPath, fileName;
        size_t pos = 0;
        Commit c;
        Tree t;
        
        snapshot = snapshot.substr(strlen(ORI_SNAPSHOT_DIRPATH) + 1);
        pos = snapshot.find('/', pos);

        if (pos == snapshot.npos) {
            c = priv->lookupSnapshot(snapshot);
            stbuf->st_uid = geteuid();
            stbuf->st_gid = getegid();
            stbuf->st_mode = 0600 | S_IFDIR;
            stbuf->st_nlink = 2;
            stbuf->st_size = 512;
            stbuf->st_blksize = 4096;
            stbuf->st_blocks = 1;
            stbuf->st_ctime = c.getTime();
            stbuf->st_mtime = c.getTime();
            return 0;
        }

        parentPath = snapshot.substr(pos);
        snapshot = snapshot.substr(0, pos);
        fileName = OriFile_Basename(parentPath);
        parentPath = OriFile_Dirname(parentPath);
        if (parentPath == "")
            parentPath = "/";

        // XXX: Enforce that this is a valid snapshot & directory path
        c = priv->lookupSnapshot(snapshot);
        t = priv->getTree(c, parentPath);

        // lookup tree
        Tree::iterator it = t.find(fileName);
        if (it == t.end())
            return -ENOENT;

        // Convert
        AttrMap *attrs = &it->second.attrs;
        struct passwd *pw = getpwnam(attrs->getAsStr(ATTR_USERNAME).c_str());

        memset(stbuf, 0, sizeof(*stbuf));
        if (it->second.type == TreeEntry::Tree) {
            stbuf->st_mode = S_IFDIR;
            stbuf->st_nlink = 2; // XXX: Correct this!
        } else {
            stbuf->st_mode = S_IFREG;
            stbuf->st_nlink = 1;
        }
        stbuf->st_mode |= attrs->getAs<mode_t>(ATTR_PERMS);
        stbuf->st_uid = pw->pw_uid;
        stbuf->st_gid = pw->pw_gid;
        stbuf->st_size = attrs->getAs<size_t>(ATTR_FILESIZE);
        stbuf->st_blocks = (stbuf->st_size + 511) / 512;
        stbuf->st_mtime = attrs->getAs<time_t>(ATTR_MTIME);
        stbuf->st_ctime = attrs->getAs<time_t>(ATTR_CTIME);

        return 0;
    }

    try {
        OriFileInfo *info = priv->getFileInfo(path);
        *stbuf = info->statInfo;
    } catch (SystemException e) {
        return -e.getErrno();
    }

    return 0;
}

static int
ori_chmod(const char *path, mode_t mode)
{
    OriPriv *priv = GetOriPriv();
    string parentPath;

    parentPath = OriFile_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    FUSE_LOG("FUSE ori_chmod(path=\"%s\")", path);

    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        return -EACCES;
    } else if (strncmp(path,
                ORI_SNAPSHOT_DIRPATH,
                strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        return -EACCES;
    }

    try {
        OriFileInfo *info = priv->getFileInfo(path);

        info->statInfo.st_mode = mode;

        OriDir *dir = priv->getDir(parentPath);
        dir->setDirty();
    } catch (SystemException e) {
        return -e.getErrno();
    }

    return 0;
}

static int
ori_chown(const char *path, uid_t uid, gid_t gid)
{
    OriPriv *priv = GetOriPriv();
    string parentPath;

    parentPath = OriFile_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    FUSE_LOG("FUSE ori_chmod(path=\"%s\")", path);

    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        return -EACCES;
    } else if (strncmp(path,
                ORI_SNAPSHOT_DIRPATH,
                strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        return -EACCES;
    }

    try {
        OriFileInfo *info = priv->getFileInfo(path);

        info->statInfo.st_uid = uid;
        info->statInfo.st_gid = gid;

        OriDir *dir = priv->getDir(parentPath);
        dir->setDirty();
    } catch (SystemException e) {
        return -e.getErrno();
    }

    return 0;
}

static int
ori_utimens(const char *path, const struct timespec tv[2])
{
    OriPriv *priv = GetOriPriv();
    string parentPath;

    parentPath = OriFile_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    FUSE_LOG("FUSE ori_utimens(path=\"%s\")", path);

    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        return -EACCES;
    } else if (strncmp(path,
                ORI_SNAPSHOT_DIRPATH,
                strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        return -EACCES;
    }

    try {
        OriFileInfo *info = priv->getFileInfo(path);

        // Ignore access times
        info->statInfo.st_mtime = tv[1].tv_sec;

        OriDir *dir = priv->getDir(parentPath);
        dir->setDirty();
    } catch (SystemException e) {
        return -e.getErrno();
    }

    return 0;
}

static int
ori_fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{
    OriFileInfo *info;

    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        return 0;
    } else if (strncmp(path,
                       ORI_SNAPSHOT_DIRPATH,
                       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
        return -EBADF;
    }

    try {
        info = priv->getFileInfo(fi->fh);
        if (info->fd == -1)
            return 0; // XXX: File is closed ignore

        return fsync(info->fd);
    } catch (SystemException &e) {
        // Fall through
    }

    return 0;
}

static struct fuse_operations ori_oper;

static void
ori_setup_ori_oper()
{
    memset(&ori_oper, 0, sizeof(struct fuse_operations));
    ori_oper.create = ori_create;

    ori_oper.init = ori_init;
    ori_oper.destroy = ori_destroy;

    ori_oper.mknod = ori_mknod;
    ori_oper.unlink = ori_unlink;
    ori_oper.symlink = ori_symlink;
    ori_oper.readlink = ori_readlink;
    ori_oper.rename = ori_rename;

    ori_oper.open = ori_open;
    ori_oper.read = ori_read;
    ori_oper.write = ori_write;
    ori_oper.truncate = ori_truncate;
    ori_oper.ftruncate = ori_ftruncate;
    ori_oper.release = ori_release;

    ori_oper.mkdir = ori_mkdir;
    ori_oper.rmdir = ori_rmdir;
    ori_oper.readdir = ori_readdir;

    ori_oper.getattr = ori_getattr;
    // XXX: fgetattr
    ori_oper.chmod = ori_chmod;
    ori_oper.chown = ori_chown;
    ori_oper.utimens = ori_utimens;

    ori_oper.fsync = ori_fsync;
    // XXX: lock (for DLM)
}

void
usage()
{
    printf("Ori Distributed Personal File System (%s) - FUSE Driver\n",
            ORI_VERSION_STR);
    printf("Usage: orifs [OPTIONS] [MOUNT POINT]\n\n");
    printf("Options:\n");
    printf("    --repo=[REPOSITORY PATH]        Repository path (required)\n");
    printf("    --clone=[REMOTE PATH]           Clone remote repository\n");
    printf("    --shallow                       Force caching shallow clone\n");
    printf("    --nocache                       Force no caching clone\n");
    printf("    --journal-none                  Disable recovery journal\n");
    printf("    --journal-async                 Asynchronous recovery journal\n");
    printf("    --journal-sync                  Synchronous recovery journal\n");
    printf("    --debug                         Enable FUSE debug mode\n");
    printf("    --help                          Print this message\n");

    printf("\nPlease report bugs to orifs-devel@stanford.edu\n");
    printf("Website: http://ori.scs.stanford.edu/\n");
}

int
main(int argc, char *argv[])
{
    int ch;
    bool createReplica = false;

    ori_setup_ori_oper();

    config.shallow = 0;
    config.nocache = 0;
    config.journal = 0;
    config.debug = 0;
    config.repoPath = "";
    config.clonePath = "";
    config.mountPoint = "";

    struct option longopts[] = {
        { "repo",           required_argument,  NULL,   'r' },
        { "clone",          required_argument,  NULL,   'c' },
        { "shallow",        no_argument,        NULL,   's' },
        { "nocache",        no_argument,        NULL,   'n' },
        { "journal-none",   no_argument,        NULL,   'x' },
        { "journal-async",  no_argument,        NULL,   'y' },
        { "journal-sync",   no_argument,        NULL,   'z' },
        { "debug",          no_argument,        NULL,   'd' },
        { "help",           no_argument,        NULL,   'h' },
        { NULL,             0,                  NULL,   0   }
    };

    while ((ch = getopt_long(argc, argv, "r:c:snxyzdh", longopts, NULL)) != -1)
    {
        switch (ch) {
            case 'r':
                config.repoPath = optarg;
                break;
            case 'c':
                config.clonePath = optarg;
                createReplica = true;
                break;
            case 's':
                config.shallow = 1;
                break;
            case 'n':
                config.nocache = 1;
                break;
            case 'x':
                config.journal = 1;
                break;
            case 'y':
                config.journal = 2;
                break;
            case 'z':
                config.journal = 3;
                break;
            case 'd':
                config.debug = 1;
                break;
            case 'h':
                usage();
                exit(0);
            default:
                printf("Unknown option!\n");
                exit(1);
        }
    }
    argc -= optind;
    argv += optind;

    if (argc != 1) {
        usage();
        return 1;
    }
    if (argc > 1) {
        printf("Too many arguments!\n");
        return 1;
    }

    config.mountPoint = argv[0];

    /*
     * If there is no repo path, then check if the repository name is the 
     * mountpoint name.  Otherwise we will generate it form the clone path.
     */
    if (config.repoPath == "" && !createReplica) {
        config.repoPath = OriFile_Basename(config.mountPoint);
    }
    if (config.repoPath == "" && createReplica) {
        string fsName = config.clonePath;
        fsName = fsName.substr(fsName.rfind("/") + 1);
        if (fsName == config.clonePath)
            fsName = fsName.substr(fsName.rfind(":") + 1);
        config.repoPath = RepoStore_GetRepoPath(fsName);
    }

    config.repoPath = RepoStore_FindRepo(config.repoPath);
    if (config.repoPath != "" && !OriFile_Exists(config.repoPath)) {
        printf("Specify the repository name or repository path!\n");
        exit(1);
    }

    /*
     * Now we do the real work of replicating and mounting the file system.
     */

    FUSE_PLOG("Ori FUSE Driver");

    if (createReplica) {
        FUSE_LOG("InstaCloning from %s", config.clonePath.c_str());
    }
    FUSE_PLOG("Opening repo at %s", config.repoPath.c_str());

    if (!OriFile_Exists(config.repoPath) && !createReplica) {
        printf("Repository does not exist! You must create one with ");
        printf("'ori init', or you may\nreplicate one from another host!\n");
        return 1;
    }

    if (createReplica) {
        if (OriFile_Exists(config.repoPath)) {
            printf("Cannot replicate onto an existing file system!\n");
            return 1;
        }

        int status = OriFile_MkDir(config.repoPath);
        if (status < 0) {
            printf("Failed to destination repository directory!\n");
            return 1;
        }

        config.repoPath = OriFile_RealPath(config.repoPath);

        printf("Creating new repository %s\n", config.repoPath.c_str());
        if (LocalRepo_Init(config.repoPath, /* bareRepo */true) != 0) {
            printf("Repository does not exist and failed to create one.\n");
            return 1;
        }

        if (config.clonePath != "") {
            if (!remoteRepo.connect(config.clonePath)) {
                printf("Failed to connect to remote repository: %s\n",
                       config.clonePath.c_str());
                return 1;
            }

            FUSE_LOG("InstaClone: Enabled!");
        }
    }
    config.repoPath = OriFile_RealPath(config.repoPath);

    if (config.shallow == 0 && createReplica) {
        try {
            LocalRepo repo;

            NOT_IMPLEMENTED(false);
            repo.open(config.repoPath);
            repo.setHead(remoteRepo->getHead());
            repo.pull(remoteRepo.get());
            repo.close();
        } catch (SystemException &e) {
            FUSE_LOG("Unexpected %s", e.what());
            throw e;
        }
    }

    try {
        if (config.shallow == 1 && createReplica) {
            string originPath = config.clonePath;

            if (!Util_IsPathRemote(originPath)) {
                originPath = OriFile_RealPath(originPath);
            }

            priv = new OriPriv(config.repoPath, originPath, remoteRepo.get());
        } else {
            priv = new OriPriv(config.repoPath);
        }
    } catch (SystemException e) {
        FUSE_LOG("Unexpected %s", e.what());
        throw e;
    }

    if (config.journal == 1) {
        priv->setJournalMode(OriJournalMode::NoJournal);
    } else if (config.journal == 2 || config.journal == 0) {
        // XXX: default
        priv->setJournalMode(OriJournalMode::AsyncJournal);
    } else if (config.journal == 3) {
        priv->setJournalMode(OriJournalMode::SyncJournal);
    } else {
        NOT_IMPLEMENTED(false);
    }

    char fuse_cmd[] = "orifs";
    char fuse_single[] = "-s";
    char fuse_debug[] = "-d";
    char fuse_mntpt[512];
    int fuse_argc = 0;
    char *fuse_argv[4] = { fuse_cmd, fuse_single };

    if (config.debug == 1) {
        cout << "Repo Path:     " << config.repoPath << endl;
        cout << "Clone Path:    " << config.clonePath << endl;
        cout << "Mount Point:   " << config.mountPoint << endl;
    }

    strncpy(fuse_mntpt, config.mountPoint.c_str(), 512);
    if (config.debug == 1)
    {
        fuse_argv[2] = fuse_debug;
        fuse_argv[3] = fuse_mntpt;
        fuse_argc = 4;
    } else {
        fuse_argv[2] = fuse_mntpt;
        fuse_argc = 3;
    }

    int status = fuse_main(fuse_argc, fuse_argv, &ori_oper, NULL);
    if (status != 0) {
        priv->cleanup();
    }

    return status;
}

