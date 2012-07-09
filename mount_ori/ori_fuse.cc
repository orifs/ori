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

#include "ori_fuse.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <dirent.h>

#include "debug.h"

typedef struct ori_priv
{
    char *datastore;
    int logfd;
} ori_priv;

static ori_priv*
ori_getpriv()
{
    return (ori_priv *)fuse_get_context()->private_data;
}

static int
ori_getattr(const char *path, struct stat *stbuf)
{
    int status;
    char fullpath[PATH_MAX];

    strcpy(fullpath, ori_getpriv()->datastore);
    strncat(fullpath, path, PATH_MAX);

    LOG("FUSE ori_getattr(path=\"%s\")", path);

    status = lstat(fullpath, stbuf);
    if (status != 0)
        return -errno;

    return 0;
}

static int
ori_statfs(const char *path, struct statvfs *statv)
{
    int status;
    char fullpath[PATH_MAX];

    strcpy(fullpath, ori_getpriv()->datastore);
    strncat(fullpath, path, PATH_MAX);

    LOG("FUSE ori_statfs(path=\"%s\")", path);

    status = statvfs(fullpath, statv);
    if (status != 0)
        return -errno;

    return 0;
}

static int
ori_open(const char *path, struct fuse_file_info *fi)
{
    int fd;
    char fullpath[PATH_MAX];

    strcpy(fullpath, ori_getpriv()->datastore);
    strncat(fullpath, path, PATH_MAX);

    LOG("FUSE ori_open(path=\"%s\")", path);

    fd = open(fullpath, fi->flags);
    if (fd < 0)
        return -errno;

    fi->fh = fd;

    return 0;
}

static int
ori_read(const char *path, char *buf, size_t size, off_t offset,
         struct fuse_file_info *fi)
{
    int status;
    LOG("FUSE ori_read(path=\"%s\", size=%d, offset=%d)", path, size, offset);

    status = pread(fi->fh, buf, size, offset);
    if (status < 0)
        return -errno;

    return status;
}

static int
ori_release(const char *path, struct fuse_file_info *fi)
{
    LOG("FUSE ori_release(path=\"%s\")", path);

    return close(fi->fh);
}

static int
ori_opendir(const char *path, struct fuse_file_info *fi)
{
    DIR *dir;
    char fullpath[PATH_MAX];

    strcpy(fullpath, ori_getpriv()->datastore);
    strncat(fullpath, path, PATH_MAX);
    
    LOG("FUSE ori_opendir(path=\"%s\")", path);

    dir = opendir(fullpath);
    if (dir == NULL) {
        LOG("opendir failed '%s': %s", path, strerror(errno));
        return -errno;
    }

    fi->fh = (intptr_t)dir;

    return 0;
}

static int
ori_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi)
{
    DIR *dir;
    struct dirent *entry;

    LOG("FUSE ori_readdir(path=\"%s\")", path);

    dir = (DIR *)(uintptr_t)fi->fh;

    entry = readdir(dir);
    if (entry == NULL) {
        return -errno;
    }

    do {
        if (filler(buf, entry->d_name, NULL, 0) != 0) {
            return -ENOMEM;
        }
    } while ((entry = readdir(dir)) != NULL);

    return 0;
}

static int
ori_releasedir(const char *path, struct fuse_file_info *fi)
{
    LOG("FUSE ori_closedir(path=\"%s\")", path);

    closedir((DIR *)(uintptr_t)fi->fh);

    return 0;
}

/**
 * Initialize the filesystem.
 */
static void *
ori_init(struct fuse_conn_info *conn)
{
    LOG("ori filesystem starting ...");
    return ori_getpriv();
}

/**
 * Cleanup the filesystem.
 */
static void
ori_destroy(void *userdata)
{
}

// C++ doesn't allow designated initializers
static struct fuse_operations ori_oper;
static void
ori_setup_ori_oper()
{
    ori_oper.getattr = ori_getattr;
    //ori_oper.readlink
    //ori_oper.getdir
    //ori_oper.mknod
    //ori_oper.mkdir
    //ori_oper.unlink
    //ori_oper.rmdir
    //ori_oper.symlink
    //ori_oper.rename
    //ori_oper.link
    //ori_oper.chmod
    //ori_oper.chown
    //ori_oper.truncate
    //ori_oper.utime
    ori_oper.open = ori_open;
    ori_oper.read = ori_read;
    //ori_oper.write
    ori_oper.statfs = ori_statfs;
    //ori_oper.flush
    ori_oper.release = ori_release;
    //ori_oper.fsync
    //ori_oper.setxattr
    //ori_oper.getxattr
    //ori_oper.listxattr
    //ori_oper.removexattr
    ori_oper.opendir = ori_opendir;
    ori_oper.readdir = ori_readdir;
    ori_oper.releasedir = ori_releasedir;
    //ori_oper.fsyncdir
    ori_oper.init = ori_init;
    ori_oper.destroy = ori_destroy;
}

int
main(int argc, char *argv[])
{
    ori_setup_ori_oper();

    // Parse command line arguments
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    mount_ori_config conf;
    mount_ori_parse_opt(&args, &conf);

    // Open the repository
    priv->datastore = realpath(argv[i], NULL);
    argv[i] = argv[i+1];
    argc--;

    return fuse_main(argc, argv, &ori_oper, priv);
}

