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

#include "localrepo.h"
#include "commit.h"
#include "debug.h"
#include "tree.h"
#include "lrucache.h"

#undef LOG
#define LOG(fmt, ...) ori_fuse_log(fmt "\n", ##__VA_ARGS__)

typedef struct ori_priv
{
    ori_priv() : datastore(NULL), logfd(0) {}

    char *datastore;
    int logfd;

    // Valid after ori_init
    LocalRepo *repo;
    Commit *head;
    Tree *headtree;

    LRUCache<std::string, Tree, 128> treecache;
} ori_priv;

static ori_priv*
ori_getpriv()
{
    return (ori_priv *)fuse_get_context()->private_data;
}

static void
ori_fuse_log(const char *what, ...)
{
    ori_priv *p = ori_getpriv();
    if (p->logfd == 0) {
        p->logfd = open("ori.log", O_CREAT|O_WRONLY|O_TRUNC, 0660);
        if (p->logfd == -1) {
            perror("open");
            exit(1);
        } 
    }

    va_list vl;
    va_start(vl, what);
    vdprintf(p->logfd, what, vl);
    va_end(vl);

    fsync(p->logfd);
}

Tree *_getTree(ori_priv *p, const std::string &hash)
{
    if (!p->treecache.hasKey(hash)) {
        Tree t;
        t.fromBlob(p->repo->getPayload(hash));
        p->treecache.put(hash, t);
    }
    return &p->treecache.get(hash);
}

int _numComponents(const char *path)
{
    size_t len = strlen(path);
    size_t cnt = 0;
    for (size_t i = 1; i < len; i++) {
        if (path[i] == '/')
            cnt++;
    }
    return cnt;
}

static TreeEntry *
_getTreeEntry(ori_priv *priv, const char *path)
{
    int numc = _numComponents(path);
    Tree *t = priv->headtree;
    TreeEntry *e = NULL;

    const char *start = path+1;
    for (int i = 0; i < numc; i++) {
        if (t == NULL) {
            // Got to leaf of tree (e.g. e is a file)
            // but still have more path components
            return NULL;
        }

        const char *end = strchr(start, '/');
        char *comp = strndup(start, end-start);
        std::map<std::string, TreeEntry>::iterator it =
            t->tree.find(comp);
        if (it == t->tree.end()) {
            free(comp);
            return NULL;
        }

        e = &(*it).second;
        if (e->type == TreeEntry::Tree) {
            t = _getTree(priv, e->hash);
        }
        else {
            t = NULL;
        }

        free(comp);
    }

    return e;
}

static int
ori_getattr(const char *path, struct stat *stbuf)
{
    LOG("FUSE ori_getattr(path=\"%s\")", path);

    ori_priv *p = ori_getpriv();
    TreeEntry *e = _getTreeEntry(p, path);
    if (e == NULL) return -ENOENT;
        
    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_uid = geteuid();
    stbuf->st_gid = getegid();

    // TODO: e->mode is not implemented yet in tree.cc
    // so just fake the mode here
    if (e->type == TreeEntry::Tree) {
        stbuf->st_mode = 0555 | S_IFDIR;
    }
    else if (e->type == TreeEntry::Blob ||
             e->type == TreeEntry::LargeBlob) {
        stbuf->st_mode = 0444 | S_IFREG;
    }
    else {
        // TreeEntry::Null
        return -EIO;
    }

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
    LOG("FUSE ori_open(path=\"%s\")", path);

    ori_priv *p = ori_getpriv();
    TreeEntry *e = _getTreeEntry(p, path);
    if (e == NULL) return -ENOENT;

    return 0;
}

static int
ori_read(const char *path, char *buf, size_t size, off_t offset,
         struct fuse_file_info *fi)
{
    LOG("FUSE ori_read(path=\"%s\", size=%d, offset=%d)", path, size, offset);

    ori_priv *p = ori_getpriv();
    TreeEntry *e = _getTreeEntry(p, path);
    if (e == NULL) return -ENOENT;

    if (e->type == TreeEntry::LargeBlob) {
        assert(false);
        return -EIO;
    }

    std::string payload = p->repo->getPayload(e->hash);
    size_t left = payload.size() - offset;
    size_t real_read = MAX(size, left);

    memcpy(buf, payload.data()+offset, real_read);

    return real_read;
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
    ori_priv *priv = ori_getpriv();
    
    priv->repo = new LocalRepo(priv->datastore);
    ori_open_log(priv->repo);

    LOG("ori filesystem starting ...");

    priv->head = new Commit();
    priv->head->fromBlob(priv->repo->getPayload(priv->repo->getHead()));

    priv->headtree = new Tree();
    priv->headtree->fromBlob(priv->repo->getPayload(
                priv->head->getTree()));

    return priv;
}

/**
 * Cleanup the filesystem.
 */
static void
ori_destroy(void *userdata)
{
    ori_priv *priv = ori_getpriv();
    delete priv->headtree;
    delete priv->head;
    delete priv->repo;
    // TODO: ?
    delete priv;
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
    //ori_oper.statfs = ori_statfs;
    //ori_oper.flush
    //ori_oper.release = ori_release;
    //ori_oper.fsync
    //ori_oper.setxattr
    //ori_oper.getxattr
    //ori_oper.listxattr
    //ori_oper.removexattr
    ori_oper.opendir = ori_opendir;
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
    ori_priv *priv = new ori_priv();
    priv->datastore = conf.repo_path;

    return fuse_main(args.argc, args.argv, &ori_oper, priv);
}

