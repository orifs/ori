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

int _numComponents(const char *path)
{
    size_t len = strlen(path);
    if (len == 1) return 0;
    size_t cnt = 1;
    for (size_t i = 1; i < len; i++) {
        if (path[i] == '/')
            cnt++;
    }
    return cnt;
}

static TreeEntry *
_getTreeEntry(ori_priv *priv, const char *path)
{
    FUSE_LOG("FUSE _getTreeEntry(path=\"%s\")", path);
    int numc = _numComponents(path);
    FUSE_LOG("num components %d", numc);

    Tree *t = priv->headtree;
    TreeEntry *e = NULL;

    size_t plen = strlen(path);
    const char *start = path+1;
    for (int i = 0; i < numc; i++) {
        if (t == NULL) {
            // Got to leaf of tree (e.g. e is a file)
            // but still have more path components
            FUSE_LOG("path leaf is a file");
            return NULL;
        }

        const char *end = strchr(start, '/');
        if (end == NULL) {
            if (start == path+plen) {
                FUSE_LOG("path too stort, too many components");
                return NULL;
            }
            end = path+plen;
        }
        char *comp = strndup(start, end-start);
        FUSE_LOG("finding %s", comp);

        std::map<std::string, TreeEntry>::iterator it =
            t->tree.find(comp);
        if (it == t->tree.end()) {
            free(comp);
            return NULL;
        }

        e = &(*it).second;
        if (e->type == TreeEntry::Tree) {
            t = priv->getTree(e->hash);
        }
        else {
            t = NULL;
        }

        free(comp);
        start = end+1;
    }

    return e;
}

static int
ori_getattr(const char *path, struct stat *stbuf)
{
    FUSE_LOG("FUSE ori_getattr(path=\"%s\")", path);

    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_uid = geteuid();
    stbuf->st_gid = getegid();

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = 0555 | S_IFDIR;
        stbuf->st_nlink = 2;
        FUSE_LOG("stat root");
        return 0;
    }

    ori_priv *p = ori_getpriv();
    TreeEntry *e = _getTreeEntry(p, path);
    if (e == NULL) {
        FUSE_LOG("getattr couldn't find file");
        return -ENOENT;
    }
        
    // TODO: e->mode is not implemented yet in tree.cc
    // so just fake the mode here
    if (e->type == TreeEntry::Tree) {
        stbuf->st_mode = 0555 | S_IFDIR;
        stbuf->st_nlink = 2;
    }
    else if (e->type == TreeEntry::Blob) {
        stbuf->st_mode = 0444 | S_IFREG;
        stbuf->st_nlink = 1;

        // Get the ObjectInfo
        ObjectInfo *oi = p->getObjectInfo(e->hash);
        stbuf->st_size = oi->payload_size;
    }
    else if (e->type == TreeEntry::LargeBlob) {
        stbuf->st_mode = 0444 | S_IFREG;
        stbuf->st_nlink = 1;

        LargeBlob *lb = p->getLargeBlob(e->hash);
        stbuf->st_size = lb->totalSize();
    }
    else {
        // TreeEntry::Null
        FUSE_LOG("entry type Null");
        return -EIO;
    }

    return 0;
}

static int
ori_statfs(const char *path, struct statvfs *statv)
{
    /*int status;
    char fullpath[PATH_MAX];

    strcpy(fullpath, ori_getpriv()->datastore);
    strncat(fullpath, path, PATH_MAX);

    FUSE_LOG("FUSE ori_statfs(path=\"%s\")", path);

    status = statvfs(fullpath, statv);
    if (status != 0)
        return -errno;*/

    return 0;
}

static int
ori_open(const char *path, struct fuse_file_info *fi)
{
    FUSE_LOG("FUSE ori_open(path=\"%s\")", path);

    ori_priv *p = ori_getpriv();
    TreeEntry *e = _getTreeEntry(p, path);
    if (e == NULL) return -ENOENT;

    return 0;
}

static int
ori_read(const char *path, char *buf, size_t size, off_t offset,
         struct fuse_file_info *fi)
{
    FUSE_LOG("FUSE ori_read(path=\"%s\", size=%d, offset=%d)", path, size, offset);

    ori_priv *p = ori_getpriv();
    TreeEntry *e = _getTreeEntry(p, path);
    if (e == NULL) return -ENOENT;

    if (e->type == TreeEntry::Blob) {
        // Read the payload to memory
        // TODO: too inefficient
        std::string payload;
        payload = p->repo->getPayload(e->hash);

        size_t left = payload.size() - offset;
        size_t real_read = MAX(size, left);

        memcpy(buf, payload.data()+offset, real_read);

        return real_read;
    }
    else if (e->type == TreeEntry::LargeBlob) {
        LargeBlob *lb = p->getLargeBlob(e->hash);
        //payload = lb->getBlob();
        FUSE_LOG("Large blobs not yet supported");
        return -EIO;
    }
}

static int
ori_release(const char *path, struct fuse_file_info *fi)
{
    FUSE_LOG("FUSE ori_release(path=\"%s\")", path);

    return close(fi->fh);
}

static int
ori_opendir(const char *path, struct fuse_file_info *fi)
{
    FUSE_LOG("FUSE ori_opendir(path=\"%s\")", path);

    return 0;
}

static int
ori_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi)
{
    FUSE_LOG("FUSE ori_readdir(path=\"%s\")", path);

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    ori_priv *p = ori_getpriv();
    Tree *t = NULL;
    if (strcmp(path, "/") == 0) {
        t = p->headtree;
    }
    else {
        TreeEntry *e = _getTreeEntry(p, path);
        if (e == NULL) return -ENOENT;
        if (e->type != TreeEntry::Tree)
            return -ENOTDIR;
        t = p->getTree(e->hash);
    }

    for (std::map<std::string, TreeEntry>::iterator it = t->tree.begin();
            it != t->tree.end();
            it++) {
        FUSE_LOG("readdir entry %s", (*it).first.c_str());
        filler(buf, (*it).first.c_str(), NULL, 0);
    }

    return 0;
}

static int
ori_releasedir(const char *path, struct fuse_file_info *fi)
{
    FUSE_LOG("FUSE ori_closedir(path=\"%s\")", path);

    closedir((DIR *)(uintptr_t)fi->fh);

    return 0;
}

/**
 * Initialize the filesystem.
 */
static void *
ori_init(struct fuse_conn_info *conn)
{
    char *datastore = (char *)fuse_get_context()->private_data;
    if (datastore == NULL) {
        FUSE_LOG("no repo selected");
        exit(1);
    }
    
    ori_priv *priv = new ori_priv(datastore);
    FUSE_LOG("ori filesystem starting ...");

    return priv;
}

/**
 * Cleanup the filesystem.
 */
static void
ori_destroy(void *userdata)
{
    ori_priv *priv = ori_getpriv();
    delete priv;

    LOG("finished");
}

// C++ doesn't allow designated initializers
static struct fuse_operations ori_oper;
static void
ori_setup_ori_oper()
{
    memset(&ori_oper, 0, sizeof(struct fuse_operations));
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
    //ori_oper.statfs = ori_statfs;
    //ori_oper.flush
    //ori_oper.release = ori_release;
    //ori_oper.fsync
    //ori_oper.setxattr
    //ori_oper.getxattr
    //ori_oper.listxattr
    //ori_oper.removexattr
    //ori_oper.opendir = ori_opendir;
    ori_oper.readdir = ori_readdir;
    //ori_oper.releasedir = ori_releasedir;
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
    memset(&conf, 0, sizeof(mount_ori_config));
    mount_ori_parse_opt(&args, &conf);

    if (conf.repo_path == NULL) {
        printf("Usage:\n");
        exit(1);
    }

    printf("Opening repo at %s\n", conf.repo_path);

    // Set the repo path
    char *datastore = realpath(conf.repo_path, NULL);
    //ori_priv *priv = new ori_priv();
    //priv->datastore = realpath(conf.repo_path, NULL);

    FUSE_LOG("main");
    FUSE_LOG("%d", args.argc);
    for (int i = 0; i < args.argc; i++)
        FUSE_LOG("%s", args.argv[i]);

    return fuse_main(args.argc, args.argv, &ori_oper, datastore);
}

