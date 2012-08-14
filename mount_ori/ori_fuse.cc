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
#include <pwd.h>
#include <grp.h>

#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <dirent.h>

#include <string>
#include <tr1/memory>

using namespace std;

#include "debug.h"
#include "util.h"
#include "remoterepo.h"
#include "fuse_cmd.h"

#define NULL_FH 0
#define ORI_CONTROL_FILEPATH "/" ORI_CONTROL_FILENAME
#define ORI_SNAPSHOT_DIRNAME ".snapshot"
#define ORI_SNAPSHOT_DIRPATH "/" ORI_SNAPSHOT_DIRNAME

mount_ori_config config;
RemoteRepo remoteRepo;

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
_getTreeEntry(ori_priv *priv, const char *cpath)
{
    /*int numc = _numComponents(path);

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

    return e;*/
    std::string path(cpath);
    if (priv->teCache.hasKey(path)) {
        return &priv->teCache.get(path);
    }
    TreeEntry te = priv->repo->lookupTreeEntry(*priv->head, path);
    if (te.type == TreeEntry::Null)
        return NULL;
    priv->teCache.put(path, te);
    return &priv->teCache.get(path);
}

static ExtendedTreeEntry *
_getETE(ori_priv *priv, const char *path)
{
    if (priv->eteCache.hasKey(path)) {
        return &priv->eteCache.get(path);
    }

    TreeEntry *te = _getTreeEntry(priv, path);
    TreeDiffEntry *tde = NULL;
    if (priv->currTreeDiff != NULL) {
        tde = priv->currTreeDiff->getLatestEntry(path);
    }

    if (te == NULL && tde == NULL) {
        return NULL;
    }
    else if (tde != NULL && (tde->type == TreeDiffEntry::DeletedFile ||
                             tde->type == TreeDiffEntry::DeletedDir)) {
        return NULL;
    }
    else if (tde != NULL && tde->type == TreeDiffEntry::Renamed) {
        NOT_IMPLEMENTED(false);
    }

    ExtendedTreeEntry ete;
    if (te != NULL) {
        ete.te = *te;
    }
    /*else {
        assert(tde != NULL);
        assert(tde->type == TreeDiffEntry::NewFile ||
               tde->type == TreeDiffEntry::NewDir);
        if (tde->type == TreeDiffEntry::NewDir) {
            ete.te.type = TreeEntry::Tree;
        }
    }*/

    if (tde != NULL) {
        if (tde->newFilename != "")
            ete.changedData = true;
        ete.tde = *tde;
        if (tde->type == TreeDiffEntry::NewDir)
            ete.te.type = TreeEntry::Tree;
        ete.te.attrs.mergeFrom(tde->newAttrs);
    }

    if (!ete.te.hasBasicAttrs()) {
        FUSE_LOG("TreeEntry missing attrs!");
        return NULL;
    }

    priv->eteCache.put(path, ete);
    return &priv->eteCache.get(path);
}


std::string
_openTempFile(ori_priv *priv, ExtendedTreeEntry *ete)
{
    priv->startWrite();
    std::string tempFile = "";
    if (ete->changedData &&
            (ete->tde.type == TreeDiffEntry::NewFile ||
                ete->tde.type == TreeDiffEntry::Modified)) {
        tempFile = ete->tde.newFilename;
    }
    else {
        tempFile = priv->currTempDir->newTempFile();
        if (tempFile == "")
            return "";
        ete->te.extractToFile(tempFile, priv->repo);
    }
    return tempFile;
}

static int
ori_getattr(const char *path, struct stat *stbuf)
{
    FUSE_LOG("FUSE ori_getattr(path=\"%s\")", path);

    memset(stbuf, 0, sizeof(struct stat));

    ori_priv *p = ori_getpriv();
    if (strcmp(path, "/") == 0) {
        stbuf->st_uid = geteuid();
        stbuf->st_gid = getegid();
        stbuf->st_mode = 0755 | S_IFDIR;
        stbuf->st_nlink = 2;
        return 0;
    } else if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        stbuf->st_uid = geteuid();
        stbuf->st_gid = getegid();
        stbuf->st_mode = 0600 | S_IFREG;
        stbuf->st_nlink = 1;
        stbuf->st_size = p->outputBuffer.size();
        return 0;
    } else if (strcmp(path, ORI_SNAPSHOT_DIRPATH) == 0) {
        stbuf->st_uid = geteuid();
        stbuf->st_gid = getegid();
        stbuf->st_mode = 0600 | S_IFDIR;
        stbuf->st_nlink = 1;
        stbuf->st_size = 512; // XXX: this is wrong
	return 0;
    } else if (strncmp(path,
		       ORI_SNAPSHOT_DIRPATH,
		       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
	string snapshot = path;
	string relPath;
	size_t pos = 0;

	snapshot = snapshot.substr(strlen(ORI_SNAPSHOT_DIRPATH) + 1);
	pos = snapshot.find('/', pos);
	if (pos == snapshot.npos) {
	    ObjectHash obj = p->repo->lookupSnapshot(snapshot);
	    Commit c = p->repo->getCommit(obj);

	    stbuf->st_uid = geteuid();
	    stbuf->st_gid = getegid();
	    stbuf->st_mode = 0600 | S_IFDIR;
	    stbuf->st_nlink = 1;
	    stbuf->st_size = 512; // XXX: this is wrong
	    stbuf->st_mtime = c.getTime();
	    stbuf->st_ctime = c.getTime();
	    return 0;
	}

	relPath = snapshot.substr(pos);
	snapshot = snapshot.substr(0, pos);

	// Lookup file
	ObjectHash obj = p->repo->lookupSnapshot(snapshot);
	if (obj.isEmpty()) {
	    LOG("FUSE snapshot '%s' was not found", snapshot.c_str());
	    return -ENOENT;
	}

	Commit c = p->repo->getCommit(obj);

	TreeEntry te = p->repo->lookupTreeEntry(c, relPath);
	if (te.type == TreeEntry::Null)
	    return -ENOENT;

	if (te.type == TreeEntry::Tree)
	{
	    stbuf->st_mode = S_IFDIR;
	    stbuf->st_nlink = 2;
	} else {
	    stbuf->st_mode = S_IFREG;
	    stbuf->st_nlink = 1;
	}

	// XXX: Mask writable attributes
	stbuf->st_mode |= te.attrs.getAs<mode_t>(ATTR_PERMS);
	struct passwd *pw = getpwnam(te.attrs.getAs<const char *>(ATTR_USERNAME));
	stbuf->st_uid = pw->pw_uid;
	stbuf->st_gid = pw->pw_gid; // TODO: process running as diff. group?
	stbuf->st_size = te.attrs.getAs<size_t>(ATTR_FILESIZE);
	stbuf->st_mtime = te.attrs.getAs<time_t>(ATTR_MTIME);
	stbuf->st_ctime = te.attrs.getAs<time_t>(ATTR_CTIME);

	return 0;
    }

    ExtendedTreeEntry *ete = _getETE(p, path);
    if (ete == NULL) return -ENOENT;

    if (ete->te.type == TreeEntry::Tree)
    {
        stbuf->st_mode = S_IFDIR;
        stbuf->st_nlink = 2;
    } else {
        stbuf->st_mode = S_IFREG;
        stbuf->st_nlink = 1;
    }

    stbuf->st_mode |= ete->te.attrs.getAs<mode_t>(ATTR_PERMS);
    struct passwd *pw = getpwnam(ete->te.attrs.getAs<const char *>(ATTR_USERNAME));
    stbuf->st_uid = pw->pw_uid;
    stbuf->st_gid = pw->pw_gid; // TODO: process running as diff. group?
    stbuf->st_size = ete->te.attrs.getAs<size_t>(ATTR_FILESIZE);
    stbuf->st_mtime = ete->te.attrs.getAs<time_t>(ATTR_MTIME);
    stbuf->st_ctime = ete->te.attrs.getAs<time_t>(ATTR_CTIME);

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

    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        return 0;
    } else if (strncmp(path,
		       ORI_SNAPSHOT_DIRPATH,
		       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
	string snapshot = path;
	string relPath;
	size_t pos = 0;

	if (fi->flags & O_WRONLY || fi->flags & O_RDWR) {
	    return -EACCES;
	}

	snapshot = snapshot.substr(strlen(ORI_SNAPSHOT_DIRPATH) + 1);
	pos = snapshot.find('/', pos);
	if (pos == snapshot.npos) {
	    return -EIO;
	}

	relPath = snapshot.substr(pos);
	snapshot = snapshot.substr(0, pos);

	// Lookup file
	ObjectHash obj = p->repo->lookupSnapshot(snapshot);
	if (obj.isEmpty()) {
	    LOG("FUSE snapshot '%s' was not found", snapshot.c_str());
	    return -ENOENT;
	}

	Commit c = p->repo->getCommit(obj);

	TreeEntry te = p->repo->lookupTreeEntry(c, relPath);

	if (te.type == TreeEntry::Null)
	{
	    return -ENOENT;
	}

	return 0;
    }

    fi->fh = NULL_FH;

    ExtendedTreeEntry *ete = _getETE(p, path);
    if (ete == NULL) return -ENOENT;

    if (fi->flags & O_WRONLY || fi->flags & O_RDWR) {
        p->startWrite();

        std::string tempFile = _openTempFile(p, ete);
        if (tempFile == "") return -EIO;

        TreeDiffEntry tde;
        tde.type = TreeDiffEntry::Modified;
        tde.filepath = path;
        tde.newFilename = tempFile;
        if (p->merge(tde)) {
            FUSE_LOG("ori_open: can't restart write here!");
            NOT_IMPLEMENTED(false);
        }

        fi->fh = open(tempFile.c_str(), fi->flags);
    }

    if (ete->changedData) {
        fi->fh = open(ete->tde.newFilename.c_str(), fi->flags);
    }

    return 0;
}

static int
ori_read_helper(ori_priv *p, TreeEntry &te, char *buf, size_t size, off_t offset)
{
    if (te.type == TreeEntry::Blob) {
        // Read the payload to memory
        // TODO: too inefficient
        std::string payload;
        payload = p->repo->getPayload(te.hash);

        size_t left = payload.size() - offset;
        size_t real_read = MIN(size, left);

        memcpy(buf, payload.data()+offset, real_read);

        return real_read;
    }
    else if (te.type == TreeEntry::LargeBlob) {
        LargeBlob *lb = p->getLargeBlob(te.hash);
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
ori_read(const char *path, char *buf, size_t size, off_t offset,
         struct fuse_file_info *fi)
{
    // TODO: deal with diffs
    FUSE_LOG("FUSE ori_read(path=\"%s\", size=%d, offset=%d)", path, size, offset);

    ori_priv *p = ori_getpriv();
    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        size_t n = p->readOutput(buf, size);
        return n;
    } else if (strncmp(path,
		       ORI_SNAPSHOT_DIRPATH,
		       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
	string snapshot = path;
	string relPath;
	size_t pos = 0;

	snapshot = snapshot.substr(strlen(ORI_SNAPSHOT_DIRPATH) + 1);
	pos = snapshot.find('/', pos);
	if (pos == snapshot.npos) {
	    return -EIO;
	}

	relPath = snapshot.substr(pos);
	snapshot = snapshot.substr(0, pos);

	// Lookup file
	ObjectHash obj = p->repo->lookupSnapshot(snapshot);
	if (obj.isEmpty()) {
	    LOG("FUSE snapshot '%s' was not found", snapshot.c_str());
	    return -ENOENT;
	}

	Commit c = p->repo->getCommit(obj);

	TreeEntry te = p->repo->lookupTreeEntry(c, relPath);

	if (te.type == TreeEntry::Null)
	{
	    return -ENOENT;
	}

	return ori_read_helper(p, te, buf, size, offset);
    }

    if (fi->fh != NULL_FH) {
        int res = pread(fi->fh, buf, size, offset);
        if (res < 0)
            return -errno;
        return res;
    }

    ExtendedTreeEntry *ete = _getETE(p, path);
    if (ete == NULL) return -ENOENT;
    assert(!ete->changedData);

    return ori_read_helper(p, ete->te, buf, size, offset);
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

    if (strncmp(path,
		ORI_SNAPSHOT_DIRPATH,
		strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
	return -EACCES;
    }

    ori_priv *p = ori_getpriv();
    p->startWrite();

    std::string tempFile = p->currTempDir->newTempFile();
    if (tempFile == "")
        return -EIO;

    TreeDiffEntry tde(path, TreeDiffEntry::NewFile);
    tde.newFilename = tempFile;
    tde.newAttrs.setCreation(0644);

    if (p->merge(tde)) {
        FUSE_LOG("committing");
        p->commitWrite();
    }

    return 0;
}

static int
ori_unlink(const char *path)
{
    FUSE_LOG("FUSE ori_unlink(path=\"%s\")", path);
    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        return -EACCES;
    } else if (strncmp(path,
		       ORI_SNAPSHOT_DIRPATH,
		       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
	return -EACCES;
    }


    // TODO: check for open files?

    ori_priv *p = ori_getpriv();
    ExtendedTreeEntry *ete = _getETE(p, path);
    if (ete == NULL) return -ENOENT;

    p->startWrite();

    TreeDiffEntry newTde;
    newTde.filepath = path;
    if (ete->te.type == TreeEntry::Tree) {
        FUSE_LOG("deleted dir");
        newTde.type = TreeDiffEntry::DeletedDir;
    }
    else {
        FUSE_LOG("deleted file");
        newTde.type = TreeDiffEntry::DeletedFile;
    }

    if (p->merge(newTde)) {
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
    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        RepoCmd *cmd = &_commands[0];
        while (true) {
            if (cmd->cmd_name == NULL) {
                char buffer[256];
                sprintf(buffer, "Unrecognized command \"%%.%lus\"\n", size);
                p->printf(buffer, buf);
                printf(buffer, buf);
                break;
            }

            if (strcmp(buf, cmd->cmd_name) == 0) {
                cmd->func(p);
                break;
            }

            cmd++;
        }
        return size;
    }

    if (fi->fh == NULL_FH)
        return -EIO;

    int res = pwrite(fi->fh, buf, size, offset);
    if (res < 0)
        return -errno;

    struct stat sb;
    if (fstat(fi->fh, &sb) < 0) {
        return -errno;
    }

    TreeDiffEntry tde(path, TreeDiffEntry::Modified);
    // TODO: do this once per commit?
    tde.newAttrs.setAs<size_t>(ATTR_FILESIZE, sb.st_size);
    if (p->merge(tde)) {
        p->commitWrite();
    }

    return res;
}

static int
ori_truncate(const char *path, off_t length)
{
    FUSE_LOG("FUSE ori_truncate(path=\"%s\", length=%s)", path, length);
    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        return 0;
    } else if (strncmp(path,
		       ORI_SNAPSHOT_DIRPATH,
		       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
	return -EACCES;
    }

    ori_priv *p = ori_getpriv();
    ExtendedTreeEntry *ete = _getETE(p, path);
    if (ete == NULL) return -ENOENT;
    if (ete->te.type == TreeEntry::Tree) return -EIO;

    p->startWrite();

    std::string tempFile = _openTempFile(p, ete);
    if (tempFile == "") return -EIO;

    int res = truncate(tempFile.c_str(), length);
    if (res < 0) return -errno;

    TreeDiffEntry newTde(path, TreeDiffEntry::Modified);
    newTde.newFilename = tempFile;
    newTde.newAttrs.setAs<size_t>(ATTR_FILESIZE, (size_t)length);
    if (p->merge(newTde)) {
        p->commitWrite();
    }

    return res;
}

static int
ori_chmod(const char *path, mode_t mode)
{
    FUSE_LOG("FUSE ori_chmod(path=\"%s\")", path);
    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        return -EACCES;
    } else if (strncmp(path,
		       ORI_SNAPSHOT_DIRPATH,
		       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
	return -EACCES;
    }

    ori_priv *p = ori_getpriv();
    ExtendedTreeEntry *ete = _getETE(p, path);
    if (ete == NULL) return -ENOENT;

    p->startWrite();

    TreeDiffEntry newTde(path, TreeDiffEntry::Modified);
    newTde.newAttrs.setAs<mode_t>(ATTR_PERMS, mode);

    if (p->merge(newTde)) {
        p->commitWrite();
    }

    return 0;
}

static int
ori_chown(const char *path, uid_t uid, gid_t gid)
{
    FUSE_LOG("FUSE ori_chown(path=\"%s\")", path);
    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        return -EACCES;
    } else if (strncmp(path,
		       ORI_SNAPSHOT_DIRPATH,
		       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
	return -EACCES;
    }

    ori_priv *p = ori_getpriv();
    ExtendedTreeEntry *ete = _getETE(p, path);
    if (ete == NULL) return -ENOENT;

    p->startWrite();

    TreeDiffEntry newTde(path, TreeDiffEntry::Modified);

    struct passwd *pw = getpwuid(uid);
    if (pw != NULL)
        newTde.newAttrs.attrs[ATTR_USERNAME] = pw->pw_name;

    struct group *grp = getgrgid(gid);
    if (grp != NULL)
        newTde.newAttrs.attrs[ATTR_GROUPNAME] = grp->gr_name;

    if (p->merge(newTde)) {
        p->commitWrite();
    }

    return 0;
}

static int
ori_utimens(const char *path, const struct timespec tv[2])
{
    FUSE_LOG("FUSE ori_utimens(path=\"%s\")", path);
    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        return -EACCES;
    } else if (strncmp(path,
		       ORI_SNAPSHOT_DIRPATH,
		       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
	return -EACCES;
    }

    ori_priv *p = ori_getpriv();
    ExtendedTreeEntry *ete = _getETE(p, path);
    if (ete == NULL) return -ENOENT;

    p->startWrite();
    // access: tv[0], modify: tv[1]
    TreeDiffEntry newTde(path, TreeDiffEntry::Modified);
    newTde.newAttrs.setAs<time_t>(ATTR_MTIME, tv[1].tv_sec);

    if (p->merge(newTde)) {
        p->commitWrite();
    }

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
        filler(buf, ORI_CONTROL_FILENAME, NULL, 0);
        filler(buf, ORI_SNAPSHOT_DIRNAME, NULL, 0);
    } else if (strcmp(path, ORI_SNAPSHOT_DIRPATH) == 0) {
	map<string, ObjectHash> snapshots = p->repo->listSnapshots();
	map<string, ObjectHash>::iterator it;

	for (it = snapshots.begin(); it != snapshots.end(); it++)
	{
	    filler(buf, (*it).first.c_str(), NULL, 0);
	}

	return 0;
    } else if (strncmp(path,
		       ORI_SNAPSHOT_DIRPATH,
		       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
	string snapshot = path;
	string relPath;
	size_t pos = 0;

	snapshot = snapshot.substr(strlen(ORI_SNAPSHOT_DIRPATH) + 1);
	pos = snapshot.find('/', pos);
	if (pos == snapshot.npos) {
	    // Snapshot root
	    ObjectHash obj = p->repo->lookupSnapshot(snapshot);
	    Commit c;

	    if (obj.isEmpty())
		return -ENOENT;
 
	    c = p->repo->getCommit(obj);
	    obj = c.getTree();
	    
	    t = p->getTree(obj);
	} else {
	    // Snapshot lookup
	    ObjectHash obj;
	    Commit c;
	    TreeEntry entry;

	    relPath = snapshot.substr(pos);
	    snapshot = snapshot.substr(0, pos - 1);

	    obj = p->repo->lookupSnapshot(snapshot);
	    if (obj.isEmpty())
		return -ENOENT;
 
	    c = p->repo->getCommit(obj);

	    entry = p->repo->lookupTreeEntry(c, relPath);
	    if (entry.type != TreeEntry::Tree)
		return -ENOTDIR;

	    t = p->getTree(entry.hash);
	}
    } else {
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

        // Check for deletions
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

    if (strncmp(path,
		ORI_SNAPSHOT_DIRPATH,
		strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
	return -EACCES;
    }

    ori_priv *p = ori_getpriv();
    p->startWrite();

    TreeDiffEntry newTde(path, TreeDiffEntry::NewDir);
    newTde.newAttrs.setCreation(0755);
    if (p->merge(newTde)) {
        p->commitWrite();
    }

    return 0;
}

static int
ori_rmdir(const char *path)
{
    FUSE_LOG("FUSE ori_rmdir(path=\"%s\")", path);

    if (strncmp(path,
		ORI_SNAPSHOT_DIRPATH,
		strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
	return -EACCES;
    }

    // TODO: is this stuff necessary?
    ori_priv *p = ori_getpriv();
    ExtendedTreeEntry *ete = _getETE(p, path);
    if (ete == NULL) return -ENOENT;

    p->startWrite();

    TreeDiffEntry newTde(path, TreeDiffEntry::DeletedDir);
    if (p->merge(newTde)) {
        p->commitWrite();
    }

    return 0;
}

static int
ori_rename(const char *from_path, const char *to_path)
{
    FUSE_LOG("FUSE ori_rename(from_path=\"%s\", to_path=\"%s\")",
            from_path, to_path);

    if (strncmp(from_path,
		ORI_SNAPSHOT_DIRPATH,
		strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
	return -EACCES;
    }

    if (strncmp(to_path,
		ORI_SNAPSHOT_DIRPATH,
		strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
	return -EACCES;
    }

    ori_priv *p = ori_getpriv();
    ExtendedTreeEntry *ete = _getETE(p, from_path);
    if (ete == NULL) return -ENOENT;

    p->startWrite();

    // Check if file exists (can't rename to existing file)
    ExtendedTreeEntry *dest_ete = _getETE(p, to_path);
    if (dest_ete != NULL) {
        TreeDiffEntry tde(to_path, dest_ete->te.type == TreeEntry::Tree ?
                TreeDiffEntry::DeletedDir : TreeDiffEntry::DeletedFile);
        p->merge(tde);
        p->commitWrite();
        p->startWrite();
    }

    TreeDiffEntry newTde(from_path, TreeDiffEntry::Renamed);
    newTde.newFilename = to_path;
    if (p->merge(newTde)) {
        p->commitWrite();
    }

    return 0;
}

/**
 * Initialize the filesystem.
 */
static void *
ori_init(struct fuse_conn_info *conn)
{
    //mount_ori_config *config =
    // (mount_ori_config *)fuse_get_context()->private_data;

    if (config.repo_path == NULL) {
        FUSE_LOG("no repo selected");
        exit(1);
    }

    if (config.clone_path != NULL) {
        // Construct remote and set head
	if (!remoteRepo.connect(config.clone_path)) {
	    FUSE_LOG("Cannot connect to source repository!");
	    exit(1);
	}
    }

    ori_priv *priv = new ori_priv(config.repo_path);

    if (config.clone_path != NULL) {
	ObjectHash revId = remoteRepo->getHead();
        priv->repo->updateHead(revId);
        FUSE_LOG("InstaClone: Updating repository head %s", revId.hex().c_str());

	string originPath = config.clone_path;
	if (!Util_IsPathRemote(originPath)) {
	    originPath = Util_RealPath(originPath);
	}
	priv->repo->addPeer("origin", originPath);
	priv->repo->setInstaClone("origin");

	priv->repo->setRemote(remoteRepo.get());
        priv->_resetHead();

        FUSE_LOG("InstaClone: Enabled!");
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
    memset(&config, 0, sizeof(mount_ori_config));
    mount_ori_parse_opt(&args, &config);

    if (config.repo_path == NULL) {
	usage();
        exit(1);
    }

    if (config.clone_path != NULL)
	printf("InstaCloning from %s\n", config.clone_path);
    printf("Opening repo at %s\n", config.repo_path);

    // Set the repo path
    if (config.clone_path == NULL)
	config.repo_path = realpath(config.repo_path, NULL);
    //ori_priv *priv = new ori_priv();
    //priv->datastore = realpath(conf.repo_path, NULL);

    FUSE_LOG("main");
    //FUSE_LOG("%d", args.argc);
    //for (int i = 0; i < args.argc; i++)
    //    FUSE_LOG("%s", args.argv[i]);

    if (config.clone_path != NULL) {
        if (!Util_FileExists(config.repo_path)) {
            mkdir(config.repo_path, 0755);
            FUSE_LOG("Creating new repository %s", config.repo_path);
            if (LocalRepo_Init(config.repo_path) != 0) {
                return 1;
            }
        }
    }

    return fuse_main(args.argc, args.argv, &ori_oper, &config);
}

