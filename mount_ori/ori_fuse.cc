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
#include "oriutil.h"
#include "remoterepo.h"
#include "fuse_cmd.h"

mount_ori_config config;
RemoteRepo remoteRepo;


int
_openTempFile(ori_priv *priv, const ExtendedTreeEntry &ete, std::string
        &tempFile, int flags, RWKey::sp repoKey)
{
    tempFile = "";
    if (ete.changedData &&
            (ete.tde.type == TreeDiffEntry::NewFile ||
                ete.tde.type == TreeDiffEntry::Modified)) {
        tempFile = ete.tde.newFilename;
    }
    else {
        tempFile = priv->currTempDir->newTempFile();
        if (tempFile == "")
            return NULL_FH;
        ete.te.extractToFile(tempFile, priv->repo);
    }

    if (tempFile == "")
        return NULL_FH;

    int fh = ::open(tempFile.c_str(), flags);
    if (fh < 0) return NULL_FH;

    priv->openedFiles.openedFile(tempFile, fh);
    return fh;
}

static int
ori_getattr(const char *path, struct stat *stbuf)
{
    FUSE_LOG("FUSE ori_getattr(path=\"%s\")", path);

    if (strcmp(path, "") == 0) {
        throw std::runtime_error("Can't getattr empty path!");
    }

    memset(stbuf, 0, sizeof(struct stat));

    ori_priv *p = ori_getpriv();
    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        RWKey::sp coKey = p->lock_cmd_output.readLock();
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
	struct passwd *pw = getpwnam(te.attrs.getAsStr(ATTR_USERNAME).c_str());
	stbuf->st_uid = pw->pw_uid;
	stbuf->st_gid = pw->pw_gid; // TODO: process running as diff. group?
	stbuf->st_size = te.attrs.getAs<size_t>(ATTR_FILESIZE);
	stbuf->st_mtime = te.attrs.getAs<time_t>(ATTR_MTIME);
	stbuf->st_ctime = te.attrs.getAs<time_t>(ATTR_CTIME);

	return 0;
    }

    if (strcmp(path, "/") == 0) {
        stbuf->st_uid = geteuid();
        stbuf->st_gid = getegid();
        stbuf->st_mode = 0755 | S_IFDIR;

        if (p->nlinkCache.hasKey("/")) {
            stbuf->st_nlink = p->nlinkCache.get("/");
        }
        else {
            stbuf->st_nlink = p->computeNLink("/");
        }
        return 0;
    }

    ExtendedTreeEntry ete;
    bool hasETE = p->getETE(path, ete);
    if (!hasETE) return -ENOENT;

    if (ete.te.type == TreeEntry::Tree)
    {
        stbuf->st_mode = S_IFDIR;
        stbuf->st_nlink = 2;
    } else if (ete.te.attrs.has(ATTR_SYMLINK) &&
            ete.te.attrs.getAs<bool>(ATTR_SYMLINK)) {
        stbuf->st_mode = S_IFLNK;
        stbuf->st_nlink = 1;
    }
    else {
        stbuf->st_mode = S_IFREG;
        stbuf->st_nlink = 1;
    }

    stbuf->st_mode |= ete.te.attrs.getAs<mode_t>(ATTR_PERMS);
    struct passwd *pw = getpwnam(ete.te.attrs.getAsStr(ATTR_USERNAME).c_str());
    stbuf->st_uid = pw->pw_uid;
    stbuf->st_gid = pw->pw_gid; // TODO: process running as diff. group?
    stbuf->st_size = ete.te.attrs.getAs<size_t>(ATTR_FILESIZE);
    stbuf->st_mtime = ete.te.attrs.getAs<time_t>(ATTR_MTIME);
    stbuf->st_ctime = ete.te.attrs.getAs<time_t>(ATTR_CTIME);

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

    ExtendedTreeEntry ete;
    bool hasETE = p->getETE(path, ete);
    if (!hasETE) return -ENOENT;

    if (ete.changedData) {
        RWKey::sp tfKey = p->openedFiles.lock_tempfiles.writeLock();
        fi->fh = open(ete.tde.newFilename.c_str(), fi->flags);

        p->openedFiles.openedFile(ete.tde.newFilename, fi->fh);
    }
    else if (fi->flags & O_WRONLY || fi->flags & O_RDWR) {
        RWKey::sp repoKey = p->startWrite();
        RWKey::sp tfKey = p->openedFiles.lock_tempfiles.writeLock();

        std::string tempFile;
        fi->fh = _openTempFile(p, ete, tempFile, fi->flags, repoKey);
        if (fi->fh == NULL_FH) return -EIO;

        tfKey.reset();

        TreeDiffEntry tde;
        tde.type = TreeDiffEntry::Modified;
        tde.filepath = path;
        tde.newFilename = tempFile;
        if (p->mergeAndCommit(tde, repoKey)) {
            FUSE_LOG("ori_open: can't restart write here!");
            NOT_IMPLEMENTED(false);
        }
    }

    return 0;
}

static int
ori_read_helper(ori_priv *p, const TreeEntry &te, char *buf, size_t size, off_t offset)
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
        const LargeBlob &lb = p->getLargeBlob(te.hash);

        size_t total = 0;
        while (total < size) {
            ssize_t res = lb.read((uint8_t*)(buf + total),
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
        RWKey::sp key = p->lock_cmd_output.writeLock();
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

    ExtendedTreeEntry ete;
    bool hasETE = p->getETE(path, ete);
    if (!hasETE) return -ENOENT;
    assert(!ete.changedData);

    return ori_read_helper(p, ete.te, buf, size, offset);
}

static int
ori_release(const char *path, struct fuse_file_info *fi)
{
    FUSE_LOG("FUSE ori_release(path=\"%s\")", path);

    ori_priv *p = ori_getpriv();
    if (fi->fh != NULL_FH) {
        RWKey::sp key = p->openedFiles.lock_tempfiles.writeLock();
        p->openedFiles.closedFile(fi->fh);
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
    RWKey::sp repoKey = p->startWrite();
    //RWKey::sp tfKey = p->openedFiles.lock_tempfiles.writeLock();

    std::string tempFile = p->currTempDir->newTempFile();
    if (tempFile == "")
        return -EIO;

    TreeDiffEntry tde(path, TreeDiffEntry::NewFile);
    tde.newFilename = tempFile;
    tde.newAttrs.setCreation(mode & (~S_IFREG));

    p->mergeAndCommit(tde, repoKey);

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
    ExtendedTreeEntry ete;
    bool hasETE = p->getETE(path, ete);
    if (!hasETE) return -ENOENT;

    RWKey::sp repoKey = p->startWrite();

    TreeDiffEntry newTde;
    newTde.filepath = path;
    if (ete.te.type == TreeEntry::Tree) {
        FUSE_LOG("deleted dir");
        newTde.type = TreeDiffEntry::DeletedDir;
    }
    else {
        FUSE_LOG("deleted file");
        newTde.type = TreeDiffEntry::DeletedFile;
    }

    p->mergeAndCommit(newTde, repoKey);
    
    return 0;
}

static int
ori_symlink(const char *target_path, const char *link_path)
{
    FUSE_LOG("FUSE ori_symlink(link_path=\"%s\", target_path=\"%s\")",
            link_path, target_path);

    if (strncmp(link_path,
		ORI_SNAPSHOT_DIRPATH,
		strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
	return -EACCES;
    }

    ori_priv *p = ori_getpriv();
    RWKey::sp repoKey = p->startWrite();
    //RWKey::sp tfKey = p->openedFiles.lock_tempfiles.writeLock();

    std::string tempFile = p->currTempDir->newTempFile();
    if (tempFile == "")
        return -EIO;
    Util_WriteFile(target_path, strlen(target_path), tempFile);

    TreeDiffEntry tde(link_path, TreeDiffEntry::NewFile);
    tde.newFilename = tempFile;
    tde.newAttrs.setCreation(0755);
    tde.newAttrs.setAs<bool>(ATTR_SYMLINK, true);
    tde.newAttrs.setAs<size_t>(ATTR_FILESIZE, strlen(target_path));

    p->mergeAndCommit(tde, repoKey);

    return 0;
}

static int
ori_readlink(const char *cpath, char *buf, size_t size)
{
    FUSE_LOG("FUSE ori_readlink(path=\"%s\")", cpath);

    ori_priv *p = ori_getpriv();
    ExtendedTreeEntry ete;
    bool hasETE = p->getETE(cpath, ete);
    if (!hasETE) return -ENOENT;

    if (!ete.te.attrs.has(ATTR_SYMLINK))
        return -EINVAL;

    if (ete.changedData) {
        int fd = ::open(ete.tde.newFilename.c_str(), O_RDONLY);
        if (fd < 0) {
            perror("ori_readlink open");
            return -EIO;
        }

        memset(buf, '\0', size);
        ssize_t result = ::read(fd, buf, size-1);
        if (result < 0) {
            perror("ori_readlink read");
            return -EIO;
        }

        close(fd);
        return 0;
    }
    else {
        memset(buf, '\0', size);
        if (ori_read_helper(p, ete.te, buf, size-1, 0) < 0) {
            return -EIO;
        }
        return 0;
    }
}

static int
ori_write(const char *path, const char *buf, size_t size, off_t offset,
         struct fuse_file_info *fi)
{
    FUSE_LOG("FUSE ori_write(path=\"%s\", size=%d, offset=%d)", path, size, offset);

    ori_priv *p = ori_getpriv();
    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        //RWKey::sp key = p->lock_cmd_output.writeLock();
        char fmt_buffer[256];
        //sprintf(fmt_buffer, "Executing command \"%%.%lus\"\n", size);
        //p->printf(fmt_buffer, buf);

        RepoCmd *cmd = &_commands[0];
        while (true) {
            if (cmd->cmd_name == NULL) {
                sprintf(fmt_buffer, "Unrecognized command \"%%.%lus\"\n", size);
                p->printf(fmt_buffer, buf);
                break;
            }

            if (strncmp(buf, cmd->cmd_name, size) == 0) {
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

    RWKey::sp repoKey = p->startWrite();
    TreeDiffEntry tde(path, TreeDiffEntry::Modified);
    tde.newAttrs.setAs<size_t>(ATTR_FILESIZE, sb.st_size);
    p->mergeAndCommit(tde, repoKey);

    return res;
}

static int
ori_truncate(const char *path, off_t length)
{
    FUSE_LOG("FUSE ori_truncate(path=\"%s\", length=%ld)", path, length);
    if (strcmp(path, ORI_CONTROL_FILEPATH) == 0) {
        return 0;
    } else if (strncmp(path,
		       ORI_SNAPSHOT_DIRPATH,
		       strlen(ORI_SNAPSHOT_DIRPATH)) == 0) {
	return -EACCES;
    }

    ori_priv *p = ori_getpriv();
    ExtendedTreeEntry ete;
    bool hasETE = p->getETE(path, ete);
    if (!hasETE) return -ENOENT;
    if (ete.te.type == TreeEntry::Tree) return -EIO;

    RWKey::sp repoKey = p->startWrite();
    RWKey::sp tfKey = p->openedFiles.lock_tempfiles.writeLock();

    std::string tempFile;
    int fh = _openTempFile(p, ete, tempFile, O_RDONLY, repoKey);
    if (fh == NULL_FH) return -EIO;
    close(fh);

    int res = truncate(tempFile.c_str(), length);
    if (res < 0) return -errno;

    tfKey.reset();

    TreeDiffEntry newTde(path, TreeDiffEntry::Modified);
    newTde.newFilename = tempFile;
    newTde.newAttrs.setAs<size_t>(ATTR_FILESIZE, (size_t)length);
    p->mergeAndCommit(newTde, repoKey);

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
    ExtendedTreeEntry ete;
    bool hasETE = p->getETE(path, ete);
    if (!hasETE) return -ENOENT;

    RWKey::sp repoKey = p->startWrite();

    TreeDiffEntry newTde(path, TreeDiffEntry::Modified);
    newTde.newAttrs.setAs<mode_t>(ATTR_PERMS, mode);

    p->mergeAndCommit(newTde, repoKey);

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
    ExtendedTreeEntry ete;
    bool hasETE = p->getETE(path, ete);
    if (!hasETE) return -ENOENT;

    RWKey::sp repoKey = p->startWrite();

    TreeDiffEntry newTde(path, TreeDiffEntry::Modified);

    struct passwd *pw = getpwuid(uid);
    if (pw != NULL)
        newTde.newAttrs.attrs[ATTR_USERNAME] = pw->pw_name;

    struct group *grp = getgrgid(gid);
    if (grp != NULL)
        newTde.newAttrs.attrs[ATTR_GROUPNAME] = grp->gr_name;

    p->mergeAndCommit(newTde, repoKey);

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
    ExtendedTreeEntry ete;
    bool hasETE = p->getETE(path, ete);
    if (!hasETE) return -ENOENT;

    RWKey::sp repoKey = p->startWrite();
    // change attrs?: tv[0], modify: tv[1]
    TreeDiffEntry newTde(path, TreeDiffEntry::Modified);
    newTde.newAttrs.setAs<time_t>(ATTR_MTIME, tv[1].tv_sec);

    p->mergeAndCommit(newTde, repoKey);

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
    Tree t;
    if (strcmp(path, "/") == 0) {
        t = *p->headtree;
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
	    
	    t = p->getTree(obj, RWKey::sp());
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

	    t = p->getTree(entry.hash, RWKey::sp());
	}
    } else {
        ExtendedTreeEntry ete;
        bool hasETE = p->getETE(path, ete);
        if (!hasETE) return -ENOENT;
        if (ete.te.type != TreeEntry::Tree)
            return -ENOTDIR;
        t = p->getTree(ete.te.hash, RWKey::sp());
    }

    std::string extPath = std::string(path) + "/";
    if (extPath.size() == 2)
        extPath.resize(1); // for root dir '//'

    RWKey::sp repoKey = p->lock_repo.readLock();

    for (std::map<std::string, TreeEntry>::iterator it = t.tree.begin();
            it != t.tree.end();
            it++) {
        //FUSE_LOG("readdir entry %s", (*it).first.c_str());

        // Check for deletions
        if (p->currTreeDiff != NULL) {
            std::string fullPath = extPath + (*it).first;

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
            if (strncmp(extPath.c_str(), tde.filepath.c_str(), extPath.size())
                    != 0) {
                continue;
            }

            if (strchr(tde.filepath.c_str()+extPath.size(), '/') != NULL)
                continue;

            if (tde.type == TreeDiffEntry::NewFile ||
                tde.type == TreeDiffEntry::NewDir) {
                filler(buf, StrUtil_Basename(tde.filepath).c_str(), NULL, 0);
            }
        }
    }

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
    RWKey::sp repoKey = p->startWrite();

    TreeDiffEntry newTde(path, TreeDiffEntry::NewDir);
    newTde.newAttrs.setCreation(mode & (~S_IFDIR));
    p->mergeAndCommit(newTde, repoKey);

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
    ExtendedTreeEntry ete;
    bool hasETE = p->getETE(path, ete);
    if (!hasETE) return -ENOENT;

    RWKey::sp repoKey = p->startWrite();

    TreeDiffEntry newTde(path, TreeDiffEntry::DeletedDir);
    p->mergeAndCommit(newTde, repoKey);

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
    ExtendedTreeEntry ete;
    bool hasETE = p->getETE(from_path, ete);
    if (!hasETE) return -ENOENT;

    // Check if file exists (can't rename to existing file)
    ExtendedTreeEntry dest_ete;
    bool hasDestETE = p->getETE(to_path, dest_ete);

    RWKey::sp repoKey = p->startWrite();
    if (hasDestETE) {
        TreeDiffEntry tde(to_path, dest_ete.te.type == TreeEntry::Tree ?
                TreeDiffEntry::DeletedDir : TreeDiffEntry::DeletedFile);
        p->mergeAndCommit(tde, repoKey);
        p->startWrite(repoKey);
    }

    TreeDiffEntry newTde(from_path, TreeDiffEntry::Renamed);
    newTde.newFilename = to_path;
    p->mergeAndCommit(newTde, repoKey);

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

    struct stat sb;
    std::string lfPath = priv->repo->getRootPath() + ORI_PATH_LOCK;
    if (lstat(lfPath.c_str(), &sb) == 0) {
        // Repo is already locked
        fprintf(stderr, "ERROR: repo at %s is currently locked\n",
                config.repo_path);
        exit(1);
    }

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
        priv->_resetHead(ObjectHash());

        FUSE_LOG("InstaClone: Enabled!");
    }

    FUSE_LOG("ori filesystem starting ...");

    // Print locks
    FUSE_LOG("lock_repo: %u", priv->lock_repo.lockNum);
    FUSE_LOG("lock_cmd_output: %u", priv->lock_cmd_output.lockNum);
    FUSE_LOG("lock_tempfiles: %u", priv->openedFiles.lock_tempfiles.lockNum);

    // Set lock ordering
    RWLock::LockOrderVector order;
    order.push_back(priv->lock_repo.lockNum);
    order.push_back(priv->openedFiles.lock_tempfiles.lockNum);
    RWLock::setLockOrder(order);

    return priv;
}

/**
 * Cleanup the filesystem.
 */
static void
ori_destroy(void *userdata)
{
    ori_priv *priv = ori_getpriv();
    priv->commitPerm();
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
    ori_oper.readlink = ori_readlink;
    //ori_oper.getdir
    ori_oper.mknod = ori_mknod;
    ori_oper.mkdir = ori_mkdir;
    ori_oper.unlink = ori_unlink;
    ori_oper.rmdir = ori_rmdir;
    ori_oper.symlink = ori_symlink;
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
    ori_oper.release = ori_release;
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

