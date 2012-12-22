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

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include <ori/debug.h>
#include <ori/oriutil.h>
#include <ori/posixexception.h>
#include <ori/rwlock.h>
#include <ori/commit.h>
#include <ori/localrepo.h>

#include <string>
#include <map>

#include "logging.h"
#include "oripriv.h"
#include "oriopt.h"

#define FSCK_A_LOT

using namespace std;

mount_ori_config config;

// Mount/Unmount

static void *
ori_init(struct fuse_conn_info *conn)
{
    OriPriv *priv;
    
    try {
        priv = new OriPriv(config.repo_path);
    } catch (PosixException e) {
        FUSE_LOG("Unexpected %s", e.what());
        throw e;
    }

    // Verify conifguration

    // Open repositories

    FUSE_LOG("Ori Filesystem starting ...");

    return priv;
}

static void
ori_destroy(void *userdata)
{
    OriPriv *priv = GetOriPriv();
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

    parentPath = StrUtil_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    try {
        OriDir *parentDir = priv->getDir(parentPath);
        OriFileInfo *info = priv->getFileInfo(path);

        if (info->isDir())
            return -EPERM;

        parentDir->remove(StrUtil_Basename(path));
        if (info->isSymlink() || info->isReg()) {
            priv->unlink(path);
        } else {
            // XXX: Support files
            assert(false);
        }
    } catch (PosixException e) {
        return -e.getErrno();
    }

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

    parentPath = StrUtil_Dirname(link_path);
    if (parentPath == "")
        parentPath = "/";

    try {
        parentDir = priv->getDir(parentPath);
    } catch (PosixException e) {
        return -e.getErrno();
    }

    OriFileInfo *info = priv->addSymlink(link_path);
    info->statInfo.st_mode |= 0755;
    info->link = target_path;
    info->statInfo.st_size = info->link.length();

    parentDir->add(StrUtil_Basename(link_path), info->id);

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
    } catch (PosixException e) {
        return -e.getErrno();
    }

    memcpy(buf, info->link.c_str(), MIN(info->link.length(), size));

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

    fromParent = StrUtil_Dirname(from_path);
    if (fromParent == "")
        fromParent = "/";
    toParent = StrUtil_Dirname(from_path);
    if (toParent == "")
        toParent = "/";

    try {
        OriDir *fromDir = priv->getDir(fromParent);
        OriDir *toDir = priv->getDir(toParent);
        OriFileInfo *info = priv->getFileInfo(from_path);
        OriFileInfo *toFile = NULL;
        OriDir *toFileDir = NULL;

        try {
            toFile = priv->getFileInfo(to_path);
        } catch (PosixException e) {
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

        // XXX: Need to support renaming directories
        if (info->isDir()) {
            FUSE_LOG("ori_rename: Directory rename attempted %s to %s",
                     from_path, to_path);
            return -EINVAL;
        }

        priv->rename(from_path, to_path);

        string to = StrUtil_Basename(from_path);
        string from = StrUtil_Basename(to_path);
        FUSE_LOG("%s %s", to.c_str(), from.c_str());

        fromDir->remove(StrUtil_Basename(from_path));
        toDir->add(StrUtil_Basename(to_path), info->id);

        // Delete previously present file
        if (toFile != NULL) {
            toFile->release();
        }
    } catch (PosixException e) {
        return -e.getErrno();
    }

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

    parentPath = StrUtil_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    try {
        parentDir = priv->getDir(parentPath);
    } catch (PosixException e) {
        return -e.getErrno();
    }

    pair<OriFileInfo *, uint64_t> info = priv->addFile(path);
    info.first->statInfo.st_mode |= mode;

    parentDir->add(StrUtil_Basename(path), info.first->id);

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

    FUSE_LOG("FUSE ori_open(path=\"%s\")", path);

    parentPath = StrUtil_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    try {
        parentDir = priv->getDir(parentPath);
        info = priv->openFile(path);
    } catch (PosixException e) {
        return -e.getErrno();
    }

    if (fi->flags & O_WRONLY || fi->flags & O_RDWR)
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
    OriFileInfo *info = priv->getFileInfo(fi->fh);
    int status;

    status = pread(info->fd, buf, size, offset);
    if (status < 0)
        return -errno;

    return status;
}

static int
ori_write(const char *path, const char *buf, size_t size, off_t offset,
         struct fuse_file_info *fi)
{
    OriPriv *priv = GetOriPriv();
    OriFileInfo *info = priv->getFileInfo(fi->fh);
    int status;

    status = pwrite(info->fd, buf, size, offset);
    if (status < 0)
        return -errno;

    // Update size
    if (info->statInfo.st_size < size + offset) {
        info->statInfo.st_size = size + offset;
        info->statInfo.st_blocks = (size + offset + (512-1))/512;
    }

    return status;
}

static int
ori_truncate(const char *path, off_t length)
{
    OriPriv *priv = GetOriPriv();
    OriFileInfo *info = priv->getFileInfo(path);

    FUSE_LOG("FUSE ori_truncate(path=\"%s\", length=%ld)", path, length);

    if (info->type == FILETYPE_TEMPORARY) {
        return truncate(info->link.c_str(), length);
    } else {
        // XXX: Not Implemented
        assert(false);
        return -EINVAL;
    }
}

static int
ori_ftruncate(const char *path, off_t length, struct fuse_file_info *fi)
{
    OriPriv *priv = GetOriPriv();
    OriFileInfo *info = priv->getFileInfo(fi->fh);

    FUSE_LOG("FUSE ori_ftruncate(path=\"%s\", length=%ld)", path, length);

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
        assert(false);
        return -EINVAL;
    }
}

static int
ori_release(const char *path, struct fuse_file_info *fi)
{
    OriPriv *priv = GetOriPriv();
    OriFileInfo *info;

    FUSE_LOG("FUSE ori_release(path=\"%s\"): fh=%ld", path, fi->fh);

    try {
        info = priv->getFileInfo(fi->fh);
    } catch (PosixException e) {
        FUSE_LOG("Unexpected in ori_release %s", e.what());
        return -e.getErrno();
    }

    // Decrement reference count (deletes temporary file for unlink)
    priv->closeFH(fi->fh);
    fi->fh = -1;

    return 0;
}

// Directory Operations

static int
ori_mkdir(const char *path, mode_t mode)
{
    OriPriv *priv = GetOriPriv();
    OriDir *parentDir;
    string parentPath;

#ifdef FSCK_A_LOT
    priv->fsck();
#endif /* FSCK_A_LOT */

    FUSE_LOG("FUSE ori_mkdir(path=\"%s\")", path);

    parentPath = StrUtil_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    try {
        parentDir = priv->getDir(parentPath);
    } catch (PosixException e) {
        return -e.getErrno();
    }

    OriFileInfo *info = priv->addDir(path);
    info->statInfo.st_mode |= mode;

    parentDir->add(StrUtil_Basename(path), info->id);

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

    parentPath = StrUtil_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    try {
        OriDir *parentDir = priv->getDir(parentPath);
        OriDir *dir = priv->getDir(path);

        if (!dir->isEmpty()) {
            OriDir::iterator it;

            FUSE_LOG("Directory not empty!");
            for (it = dir->begin(); it != dir->end(); it++) {
                FUSE_LOG("DIR: %s\n", it->first.c_str());
            }

            return -ENOTEMPTY;
        }

        parentDir->remove(StrUtil_Basename(path));
        priv->rmDir(path);
    } catch (PosixException e) {
        return -e.getErrno();
    }

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

    try {
        dir = priv->getDir(path);
    } catch (PosixException e) {
        return -e.getErrno();
    }

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    try {
        for (it = dir->begin(); it != dir->end(); it++) {
            OriFileInfo *info = priv->getFileInfo(dirPath + (*it).first);

            filler(buf, (*it).first.c_str(), &info->statInfo, 0);
        }
    } catch (PosixException e) {
        FUSE_LOG("Unexpected %s", e.what());
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

    try {
        OriFileInfo *info = priv->getFileInfo(path);
        *stbuf = info->statInfo;
    } catch (PosixException e) {
        return -e.getErrno();
    }

    return 0;
}

static int
ori_chmod(const char *path, mode_t mode)
{
    OriPriv *priv = GetOriPriv();
    string parentPath;

    parentPath = StrUtil_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    FUSE_LOG("FUSE ori_chmod(path=\"%s\")", path);

    try {
        OriFileInfo *info = priv->getFileInfo(path);

        info->statInfo.st_mode = mode;

        OriDir *dir = priv->getDir(parentPath);
        dir->setDirty();
    } catch (PosixException e) {
        return -e.getErrno();
    }

    return 0;
}

static int
ori_chown(const char *path, uid_t uid, gid_t gid)
{
    OriPriv *priv = GetOriPriv();
    string parentPath;

    parentPath = StrUtil_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    FUSE_LOG("FUSE ori_chmod(path=\"%s\")", path);

    try {
        OriFileInfo *info = priv->getFileInfo(path);

        info->statInfo.st_uid = uid;
        info->statInfo.st_gid = gid;

        OriDir *dir = priv->getDir(parentPath);
        dir->setDirty();
    } catch (PosixException e) {
        return -e.getErrno();
    }

    return 0;
}

static int
ori_utimens(const char *path, const struct timespec tv[2])
{
    OriPriv *priv = GetOriPriv();
    string parentPath;

    parentPath = StrUtil_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    FUSE_LOG("FUSE ori_utimens(path=\"%s\")", path);

    try {
        OriFileInfo *info = priv->getFileInfo(path);

        // Ignore access times
        info->statInfo.st_mtime = tv[1].tv_sec;

        OriDir *dir = priv->getDir(parentPath);
        dir->setDirty();
    } catch (PosixException e) {
        return -e.getErrno();
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
    ori_oper.chmod = ori_chmod;
    ori_oper.chown = ori_chown;
    ori_oper.utimens = ori_utimens;
}

void
usage()
{
    printf("Usage:\n");
    printf("mount_ori -o repo=[REPOSITORY PATH] [MOUNT POINT]\n");
    printf("mount_ori -o clone=[REMOTE PATH],repo=[REPOSITORY PATH] [MOUNT POINT]\n");
}

int
main(int argc, char *argv[])
{
    ori_setup_ori_oper();
    umask(0);

    // Parse arguments
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    mount_ori_parse_opt(&args, &config);

    if (config.repo_path == NULL) {
        usage();
        exit(1);
    }

    printf("Opening repo at %s\n", config.repo_path);

    return fuse_main(args.argc, args.argv, &ori_oper, NULL);
}

