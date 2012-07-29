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

#include <string>
#include <tr1/memory>

using namespace std;

#include "debug.h"
#include "util.h"
#include "sshclient.h"
#include "sshrepo.h"
#include "httpclient.h"
#include "httprepo.h"

#define NULL_FH 0

#define CONTROL_FILENAME ".ori_control"
#define CONTROL_FILEPATH "/" CONTROL_FILENAME

std::tr1::shared_ptr<Repo> remoteRepo;

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

static TreeDiffEntry *
_getTreeDiffEntry(ori_priv *priv, const char *path)
{
    if (priv->currTreeDiff != NULL) {
        TreeDiffEntry *tde = priv->currTreeDiff->getLatestEntry(path);
        printf("_getTreeDiffEntry %s %p\n", path, tde);
        return tde;
    }
    return NULL;
}

static TreeEntry *
_getTreeEntry(ori_priv *priv, const char *path)
{
    int numc = _numComponents(path);

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
        //FUSE_LOG("finding %s", comp);

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

std::string
_openTempFile(ori_priv *priv, TreeEntry *e, TreeDiffEntry *tde)
{
    priv->startWrite();
    std::string tempFile = "";
    if (tde != NULL &&
            (tde->type == TreeDiffEntry::NewFile ||
                tde->type == TreeDiffEntry::Modified)) {
        tempFile = tde->newFilename;
    }
    else if (tde == NULL && e != NULL) {
        tempFile = priv->currTempDir->newTempFile();
        if (tempFile == "")
            return "";
        e->extractToFile(tempFile, priv->repo);
    }
    else {
        FUSE_LOG("ori_open: Not sure how we got here");
        NOT_IMPLEMENTED(false);
    }
    return tempFile;
}

static int
ori_getattr(const char *path, struct stat *stbuf)
{
    FUSE_LOG("FUSE ori_getattr(path=\"%s\")", path);

    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_uid = geteuid();
    stbuf->st_gid = getegid();

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = 0755 | S_IFDIR;
        stbuf->st_nlink = 2;
        return 0;
    }
    else if (strcmp(path, CONTROL_FILEPATH) == 0) {
        stbuf->st_mode = 0200 | S_IFREG;
        stbuf->st_nlink = 1;
        stbuf->st_size = 0;
        return 0;
    }

    ori_priv *p = ori_getpriv();
    TreeEntry *e = _getTreeEntry(p, path);
    TreeDiffEntry *tde = _getTreeDiffEntry(p, path);
    if (e == NULL && tde == NULL) {
        FUSE_LOG("getattr couldn't find file");
        return -ENOENT;
    }

    if (tde != NULL && (tde->type == TreeDiffEntry::DeletedFile ||
                        tde->type == TreeDiffEntry::DeletedDir)) {
        FUSE_LOG("%s was deleted", path);
        return -ENOENT;
    }

    if ((e != NULL && e->type == TreeEntry::Tree) ||
            (tde != NULL && tde->type == TreeDiffEntry::NewDir))
    {
        FUSE_LOG("getattr directory %p %p", e, tde);
        stbuf->st_mode = 0755 | S_IFDIR;
        stbuf->st_nlink = 2;
        return 0;
    }

    if (tde != NULL &&
            (tde->type == TreeDiffEntry::NewFile ||
                tde->type == TreeDiffEntry::Modified)) {
        int res = lstat(tde->newFilename.c_str(), stbuf);
        if (res < 0) return -errno;
        return res;
    }

    if (e == NULL) {
        FUSE_LOG("ori_getattr: not sure how we got here");
        return -EIO;
    }

    ///////////////////////
    // Files

    // TODO: e->mode is not implemented yet in tree.cc
    // so just fake the mode here
    if (e->type == TreeEntry::Blob) {
        stbuf->st_mode = 0644 | S_IFREG;
        stbuf->st_nlink = 1;

        // Get the ObjectInfo
        ObjectInfo *oi = p->getObjectInfo(e->hash);
        stbuf->st_size = oi->payload_size;
    }
    else if (e->type == TreeEntry::LargeBlob) {
        stbuf->st_mode = 0644 | S_IFREG;
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
    if (strcmp(path, CONTROL_FILEPATH) == 0) {
        return 0;
    }

    fi->fh = NULL_FH;

    ori_priv *p = ori_getpriv();
    TreeEntry *e = _getTreeEntry(p, path);
    TreeDiffEntry *tde = _getTreeDiffEntry(p, path);
    if (e == NULL && tde == NULL) return -ENOENT;
    if (tde != NULL && tde->type == TreeDiffEntry::DeletedFile)
        return -ENOENT;

    if (fi->flags & O_WRONLY || fi->flags & O_RDWR) {
        p->startWrite();

        std::string tempFile = _openTempFile(p, e, tde);
        if (tempFile == "") return -EIO;

        TreeDiffEntry tde;
        tde.type = TreeDiffEntry::Modified;
        tde.filepath = path;
        tde.newFilename = tempFile;
        if (p->currTreeDiff->merge(tde)) {
            FUSE_LOG("ori_open: can't restart write here!");
            NOT_IMPLEMENTED(false);
        }

        fi->fh = open(tempFile.c_str(), fi->flags);
    }

    return 0;
}

static int
ori_read(const char *path, char *buf, size_t size, off_t offset,
         struct fuse_file_info *fi)
{
    // TODO: deal with diffs
    FUSE_LOG("FUSE ori_read(path=\"%s\", size=%d, offset=%d)", path, size, offset);
    if (strcmp(path, CONTROL_FILEPATH) == 0) {
        return 0;
    }

    if (fi->fh != NULL_FH) {
        int res = pread(fi->fh, buf, size, offset);
        if (res < 0)
            return -errno;
        return res;
    }

    ori_priv *p = ori_getpriv();
    TreeEntry *e = _getTreeEntry(p, path);
    if (e == NULL) return -ENOENT;

    if (e->type == TreeEntry::Blob) {
        // Read the payload to memory
        // TODO: too inefficient
        std::string payload;
        payload = p->repo->getPayload(e->hash);

        size_t left = payload.size() - offset;
        size_t real_read = MIN(size, left);

        memcpy(buf, payload.data()+offset, real_read);

        return real_read;
    }
    else if (e->type == TreeEntry::LargeBlob) {
        LargeBlob *lb = p->getLargeBlob(e->hash);
        //lb->extractFile("temp.tst");
        size_t total = 0;
        while (total < size) {
            ssize_t res = lb->read((uint8_t*)(buf + total),
                    size - total, offset + total);
            if (res <= 0) return res;
            total += res;
        }
        return total;
    }

    return -EIO;
}

static int
ori_release(const char *path, struct fuse_file_info *fi)
{
    FUSE_LOG("FUSE ori_release(path=\"%s\")", path);

    if (fi->fh != NULL_FH) {
        return close(fi->fh);
    }
    return 0;
}

static int
ori_mknod(const char *path, mode_t mode, dev_t dev)
{
    FUSE_LOG("FUSE ori_mknod(path=\"%s\")", path);

    ori_priv *p = ori_getpriv();
    p->startWrite();

    std::string tempFile = p->currTempDir->newTempFile();
    if (tempFile == "")
        return -EIO;

    TreeDiffEntry tde;
    tde.type = TreeDiffEntry::NewFile;
    tde.filepath = path;
    tde.newFilename = tempFile;

    if (p->currTreeDiff->merge(tde)) {
        FUSE_LOG("committing");
        p->commitWrite();
    }

    return 0;
}

static int
ori_unlink(const char *path)
{
    FUSE_LOG("FUSE ori_unlink(path=\"%s\")", path);
    if (strcmp(path, CONTROL_FILEPATH) == 0) {
        return -EACCES;
    }

    // TODO: check for open files?

    ori_priv *p = ori_getpriv();
    TreeEntry *e = _getTreeEntry(p, path);
    TreeDiffEntry *tde = _getTreeDiffEntry(p, path);
    if (e == NULL && tde == NULL) return -ENOENT;
    if (tde != NULL && tde->type == TreeDiffEntry::DeletedFile)
        return -ENOENT;

    p->startWrite();

    TreeDiffEntry newTde;
    newTde.filepath = path;
    if ((e != NULL && e->type == TreeEntry::Tree) ||
        (tde != NULL && tde->type == TreeDiffEntry::NewDir)) {
        FUSE_LOG("deleted dir");
        newTde.type = TreeDiffEntry::DeletedDir;
    }
    else {
        FUSE_LOG("deleted file");
        newTde.type = TreeDiffEntry::DeletedFile;
    }

    if (p->currTreeDiff->merge(newTde)) {
        p->commitWrite();
    }
    
    return 0;
}

static int
ori_write(const char *path, const char *buf, size_t size, off_t offset,
         struct fuse_file_info *fi)
{
    FUSE_LOG("FUSE ori_write(path=\"%s\", size=%d, offset=%d)", path, size, offset);

    ori_priv *p = ori_getpriv();
    if (strcmp(path, CONTROL_FILEPATH) == 0) {
        // TODO
        if (strncmp(buf, "commit", 6) == 0) {
            p->commitWrite();
        }
        return size;
    }

    if (fi->fh == NULL_FH)
        return -EIO;

    int res = pwrite(fi->fh, buf, size, offset);
    if (res < 0)
        return -errno;
    return res;
}

static int
ori_truncate(const char *path, off_t length)
{
    FUSE_LOG("FUSE ori_truncate(path=\"%s\", length=%s)", path, length);
    if (strcmp(path, CONTROL_FILEPATH) == 0) {
        return 0;
    }

    ori_priv *p = ori_getpriv();
    TreeEntry *e = _getTreeEntry(p, path);
    TreeDiffEntry *tde = _getTreeDiffEntry(p, path);
    if (e == NULL && tde == NULL) return -ENOENT;
    if (tde != NULL && tde->type == TreeDiffEntry::DeletedFile)
        return -ENOENT;

    p->startWrite();

    std::string tempFile = _openTempFile(p, e, tde);
    if (tempFile == "") return -EIO;

    int res = truncate(tempFile.c_str(), length);
    if (res < 0) return -errno;

    TreeDiffEntry newTde;
    newTde.filepath = path;
    newTde.type = TreeDiffEntry::Modified;
    newTde.newFilename = tempFile;
    if (p->currTreeDiff->merge(newTde)) {
        p->commitWrite();
    }

    return res;
}

static int
ori_chmod(const char *path, mode_t mode)
{
    FUSE_LOG("FUSE ori_chmod(path=\"%s\")", path);
    if (strcmp(path, CONTROL_FILEPATH) == 0) {
        return -EACCES;
    }
    // TODO
    return 0;
}

static int
ori_chown(const char *path, uid_t uid, gid_t gid)
{
    FUSE_LOG("FUSE ori_chown(path=\"%s\")", path);
    if (strcmp(path, CONTROL_FILEPATH) == 0) {
        return -EACCES;
    }
    // TODO
    return 0;
}

static int
ori_utimens(const char *path, const struct timespec tv[2])
{
    FUSE_LOG("FUSE ori_utimens(path=\"%s\")", path);
    if (strcmp(path, CONTROL_FILEPATH) == 0) {
        return -EACCES;
    }
    // TODO
    return 0;
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
        filler(buf, CONTROL_FILENAME, NULL, 0);
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
        // Check for deletions
        FUSE_LOG("readdir entry %s", (*it).first.c_str());
        if (p->currTreeDiff != NULL) {
            std::string fullPath = std::string(path) + "/";
            if (fullPath.size() == 2)
                fullPath.resize(1); // for root dir '//'
            fullPath += (*it).first;
            FUSE_LOG("fullpath %s", fullPath.c_str());

            TreeDiffEntry *tde = p->currTreeDiff->getLatestEntry(fullPath);
            if (tde != NULL && (tde->type == TreeDiffEntry::DeletedFile ||
                                tde->type == TreeDiffEntry::DeletedDir)) {
                continue;
            }
        }

        filler(buf, (*it).first.c_str(), NULL, 0);
    }

    // Check for additions
    if (p->currTreeDiff != NULL) {
        for (size_t i = 0; i < p->currTreeDiff->entries.size(); i++) {
            const TreeDiffEntry &tde = p->currTreeDiff->entries[i];
            if (tde.type == TreeDiffEntry::NewFile ||
                tde.type == TreeDiffEntry::NewDir) {
                filler(buf, StrUtil_Basename(tde.filepath).c_str(), NULL, 0);
            }
        }
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

static int
ori_mkdir(const char *path, mode_t mode)
{
    FUSE_LOG("FUSE ori_mkdir(path=\"%s\")", path);

    ori_priv *p = ori_getpriv();
    p->startWrite();

    TreeDiffEntry newTde;
    newTde.type = TreeDiffEntry::NewDir;
    newTde.filepath = path;
    if (p->currTreeDiff->merge(newTde)) {
        p->commitWrite();
    }

    return 0;
}

static int
ori_rmdir(const char *path)
{
    FUSE_LOG("FUSE ori_rmdir(path=\"%s\")", path);

    // TODO: is this stuff necessary?
    ori_priv *p = ori_getpriv();
    TreeEntry *e = _getTreeEntry(p, path);
    TreeDiffEntry *tde = _getTreeDiffEntry(p, path);
    if (e == NULL && tde == NULL) return -ENOENT;
    if (tde != NULL && tde->type == TreeDiffEntry::DeletedDir)
        return -ENOENT;

    p->startWrite();

    TreeDiffEntry newTde;
    newTde.type = TreeDiffEntry::DeletedDir;
    newTde.filepath = path;
    if (p->currTreeDiff->merge(newTde)) {
        p->commitWrite();
    }

    return 0;
}

static int
ori_rename(const char *from_path, const char *to_path)
{
    ori_priv *p = ori_getpriv();
    TreeEntry *e = _getTreeEntry(p, from_path);
    TreeDiffEntry *tde = _getTreeDiffEntry(p, from_path);
    if (e == NULL && tde == NULL) return -ENOENT;
    if (tde != NULL && tde->type == TreeDiffEntry::DeletedDir)
        return -ENOENT;

    /*bool is_dir = (e != NULL && e->type == TreeEntry::Tree) ||
        (tde != NULL && tde->type == TreeDiffEntry::NewDir);*/

    p->startWrite();

    TreeDiffEntry newTde;
    newTde.type = TreeDiffEntry::Renamed;
    newTde.filepath = from_path;
    newTde.newFilename = to_path;
    if (p->currTreeDiff->merge(newTde)) {
        p->commitWrite();
    }

    return 0;
}

/// XXX: Copied from ori/repo_cmd.cc
int
getRepoFromURL(const string &url,
	       std::tr1::shared_ptr<Repo> &r,
	       std::tr1::shared_ptr<HttpClient> &hc,
	       std::tr1::shared_ptr<SshClient> &sc)
{
    if (Util_IsPathRemote(url.c_str())) {
	if (strncmp(url.c_str(), "http://", 7) == 0) {
	    hc.reset(new HttpClient(url));
	    r.reset(new HttpRepo(hc.get()));
	    hc->connect();
	} else {
	    sc.reset(new SshClient(url));
	    r.reset(new SshRepo(sc.get()));
	    sc->connect();
	}
    } else {
	r.reset(new LocalRepo(url));
	((LocalRepo *)r.get())->open(url);
    }

    return 0; // TODO: errors?
}


/**
 * Initialize the filesystem.
 */
static void *
ori_init(struct fuse_conn_info *conn)
{
    mount_ori_config *config =
		    (mount_ori_config *)fuse_get_context()->private_data;
    if (config->repo_path == NULL) {
        FUSE_LOG("no repo selected");
        exit(1);
    }

    if (config->clone_path != NULL) {
	LocalRepo dstRepo;
	std::tr1::shared_ptr<HttpClient> httpClient;
	std::tr1::shared_ptr<SshClient> sshClient;

	LocalRepo_Init(config->repo_path);

	// Construct remote and set head
	getRepoFromURL(config->clone_path, remoteRepo, httpClient, sshClient);

	dstRepo.open(config->repo_path);
	dstRepo.updateHead(remoteRepo->getHead());
	dstRepo.close();
    }

    ori_priv *priv = new ori_priv(config->repo_path);

    if (config->clone_path != NULL) {
	priv->repo->setRemote(remoteRepo.get());
    }

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
    priv->commitWrite();
    delete priv;

    FUSE_LOG("finished");
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
    ori_oper.mknod = ori_mknod;
    ori_oper.mkdir = ori_mkdir;
    ori_oper.unlink = ori_unlink;
    ori_oper.rmdir = ori_rmdir;
    //ori_oper.symlink
    ori_oper.rename = ori_rename;
    //ori_oper.link
    ori_oper.chmod = ori_chmod;
    ori_oper.chown = ori_chown;
    ori_oper.truncate = ori_truncate;
    //ori_oper.utime
    ori_oper.open = ori_open;
    ori_oper.read = ori_read;
    ori_oper.write = ori_write;
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

    // Parse command line arguments
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    mount_ori_config conf;
    memset(&conf, 0, sizeof(mount_ori_config));
    mount_ori_parse_opt(&args, &conf);

    if (conf.repo_path == NULL && conf.clone_path == NULL) {
	usage();
        exit(1);
    }

    printf("Opening repo at %s\n", conf.repo_path);

    // Set the repo path
    conf.repo_path = realpath(conf.repo_path, NULL);
    //ori_priv *priv = new ori_priv();
    //priv->datastore = realpath(conf.repo_path, NULL);

    FUSE_LOG("main");
    FUSE_LOG("%d", args.argc);
    for (int i = 0; i < args.argc; i++)
        FUSE_LOG("%s", args.argv[i]);

    return fuse_main(args.argc, args.argv, &ori_oper, &conf);
}

