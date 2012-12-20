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

#include <string>
#include <map>

#include "logging.h"
#include "oripriv.h"

using namespace std;

// Mount/Unmount

static void *
ori_init(struct fuse_conn_info *conn)
{
    OriPriv *priv = new OriPriv();

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
}

static int
ori_symlink(const char *target_path, const char *link_path)
{
}

static int
ori_readlink(const char *cpath, char *buf, size_t size)
{
}

static int
ori_rename(const char *from_path, const char *to_path)
{
}

// File IO

static int
ori_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
}

static int
ori_open(const char *path, struct fuse_file_info *fi)
{
}

static int
ori_read(const char *path, char *buf, size_t size, off_t offset,
         struct fuse_file_info *fi)
{
}

static int
ori_write(const char *path, const char *buf, size_t size, off_t offset,
         struct fuse_file_info *fi)
{
}

static int
ori_truncate(const char *path, off_t length)
{
}

static int
ori_release(const char *path, struct fuse_file_info *fi)
{
}

// Directory Operations

static int
ori_mkdir(const char *path, mode_t mode)
{
    OriPriv *priv = GetOriPriv();
    OriDir *parentDir;
    string parentPath;

    FUSE_LOG("FUSE ori_mkdir(path=\"%s\")", path);

    parentPath = StrUtil_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    try {
        parentDir = &priv->getDir(parentPath);
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

    FUSE_LOG("FUSE ori_rmdir(path=\"%s\")", path);

    parentPath = StrUtil_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    try {
        OriDir *parentDir = &priv->getDir(parentPath);
        OriDir *dir = &priv->getDir(path);

        if (!dir->isEmpty())
            return -ENOTEMPTY;

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
    OriDir dir;
    OriDir::iterator it;
    string dirPath = path;

    if (dirPath != "/")
        dirPath += "/";

    FUSE_LOG("FUSE ori_readdir(path=\"%s\", offset=%ld)", path, offset);

    try {
        dir = priv->getDir(path);
    } catch (PosixException e) {
        return -e.getErrno();
    }

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    try {
        for (it = dir.begin(); it != dir.end(); it++) {
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
    string dirPath = StrUtil_Dirname(path);

    if (dirPath != "/")
        dirPath += "/";

    FUSE_LOG("FUSE ori_chmod(path=\"%s\")", path);

    try {
        OriFileInfo *info = priv->getFileInfo(path);

        info->statInfo.st_mode = mode;

        OriDir *dir = &priv->getDir(dirPath);
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
    string dirPath = path;

    if (dirPath != "/")
        dirPath += "/";

    FUSE_LOG("FUSE ori_chmod(path=\"%s\")", path);

    try {
        OriFileInfo *info = priv->getFileInfo(path);

        info->statInfo.st_uid = uid;
        info->statInfo.st_gid = gid;

        OriDir *dir = &priv->getDir(dirPath);
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
    string dirPath = path;

    if (dirPath != "/")
        dirPath += "/";

    FUSE_LOG("FUSE ori_utimens(path=\"%s\")", path);

    try {
        OriFileInfo *info = priv->getFileInfo(path);

        // Ignore access times
        info->statInfo.st_mtime = tv[1].tv_sec;

        OriDir *dir = &priv->getDir(dirPath);
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
    ori_oper.release = ori_release;

    ori_oper.mkdir = ori_mkdir;
    ori_oper.rmdir = ori_rmdir;
    ori_oper.readdir = ori_readdir;

    ori_oper.getattr = ori_getattr;
    ori_oper.chmod = ori_chmod;
    ori_oper.chown = ori_chown;
    ori_oper.utimens = ori_utimens;
}

int
main(int argc, char *argv[])
{
    ori_setup_ori_oper();
    umask(0);
    return fuse_main(argc, argv, &ori_oper, NULL);
}

