/*
 * Copyright (c) 2012-2013 Stanford University
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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include <string>
#include <map>
#include <algorithm>
#include <boost/tr1/memory.hpp>
#include <oriutil/oritr1.h>

#include <oriutil/debug.h>
#include <oriutil/orifile.h>
#include <oriutil/scan.h>
#include <oriutil/systemexception.h>
#include <oriutil/rwlock.h>
#include <oriutil/objecthash.h>
#include <ori/commit.h>
#include <ori/localrepo.h>
#include <ori/treediff.h>

#include "logging.h"
#include "oricmd.h"
#include "oripriv.h"
#include "oriopt.h"
#include "server.h"

using namespace std;
using namespace std::tr1;

// XXX: Hacky remove dependence
extern mount_ori_config config;

void
OriFileInfo::loadAttr(const AttrMap &attrs)
{
    struct passwd *pw = getpwnam(attrs.getAsStr(ATTR_USERNAME).c_str());

    // XXX: Choose sane defaults
    ASSERT(pw != NULL);

    if (statInfo.st_mode != S_IFDIR) {
        bool isSymlink = false;

        if (attrs.has(ATTR_SYMLINK)) {
            isSymlink = attrs.getAs<bool>(ATTR_SYMLINK);
        }
    
        if (isSymlink) {
            statInfo.st_mode = S_IFLNK;
            statInfo.st_nlink = 1;
        } else {
            statInfo.st_mode = S_IFREG;
            statInfo.st_nlink = 1;
        }
    }

    statInfo.st_mode |= attrs.getAs<mode_t>(ATTR_PERMS);
    statInfo.st_uid = pw->pw_uid;
    statInfo.st_gid = pw->pw_gid;
    statInfo.st_size = attrs.getAs<size_t>(ATTR_FILESIZE);
    statInfo.st_blocks = (statInfo.st_size + 511) / 512;
    statInfo.st_mtime = attrs.getAs<time_t>(ATTR_MTIME);
    statInfo.st_ctime = attrs.getAs<time_t>(ATTR_CTIME);
}

void
OriFileInfo::storeAttr(AttrMap *attrs) const
{
    ASSERT(attrs != NULL);

    struct passwd *pw = getpwuid(statInfo.st_uid);
    struct group *grp = getgrgid(statInfo.st_gid);

    if (pw != NULL)
        attrs->setAsStr(ATTR_USERNAME, pw->pw_name);
    else
        attrs->setAsStr(ATTR_USERNAME, "nobody");

    if (grp != NULL)
        attrs->setAsStr(ATTR_GROUPNAME, grp->gr_name);
    else
        attrs->setAsStr(ATTR_GROUPNAME, "nogroup");

    if (isSymlink())
        attrs->setAs<bool>(ATTR_SYMLINK, true);
    else
        attrs->setAs<bool>(ATTR_SYMLINK, false);

    attrs->setAs<mode_t>(ATTR_PERMS, statInfo.st_mode & 0777);
    attrs->setAs<size_t>(ATTR_FILESIZE, statInfo.st_size);
    attrs->setAs<time_t>(ATTR_MTIME, statInfo.st_mtime);
    attrs->setAs<time_t>(ATTR_CTIME, statInfo.st_ctime);
}

OriPriv::OriPriv(const std::string &repoPath,
                 const string &origin,
                 Repo *remoteRepo)
{
    repo = new LocalRepo(repoPath);
    nextId = ORIPRIVID_INVALID + 1;
    nextFH = 1;

    try {
        repo->open();
        if (ori_open_log(repo->getLogPath()) < 0)
            printf("Couldn't open log!\n");
    } catch (exception &e) {
        cout << e.what() << endl;
        printf("Failed to open ori repository please check the path!\n");
        exit(1);
    }

    if (remoteRepo) {
        ASSERT(origin != "");
        repo->addPeer("origin", origin);
        repo->setInstaClone("origin", true);
        if (config.nocache == 1) {
            repo->setRemoteFlags(false);
        }
        repo->setRemote(remoteRepo);
        ObjectHash head = remoteRepo->getHead();
        if (!head.isEmpty())
            repo->updateHead(head);
    }

    RWLock::LockOrderVector order;
    order.push_back(ioLock.lockNum);
    order.push_back(nsLock.lockNum);
    RWLock::setLockOrder(order);

    reset();

    // Create root directory info
    OriFileInfo *dirInfo = new OriFileInfo();
    dirInfo->statInfo.st_uid = geteuid();
    dirInfo->statInfo.st_gid = getegid();
    dirInfo->statInfo.st_mode = 0755 | S_IFDIR;
    dirInfo->statInfo.st_nlink = 2;
    dirInfo->statInfo.st_blksize = 4096;
    dirInfo->statInfo.st_blocks = 1;
    dirInfo->statInfo.st_size = 512;
    dirInfo->id = generateId();
    if (head.isEmpty()) {
        time_t now = time(NULL);
        dirInfo->statInfo.st_mtime = now;
        dirInfo->statInfo.st_ctime = now;
        dirInfo->type = FILETYPE_DIRTY;
        dirInfo->dirLoaded = true;
        dirs[dirInfo->id] = new OriDir();
    } else {
        dirInfo->statInfo.st_mtime = headCommit.getTime();
        dirInfo->statInfo.st_ctime = headCommit.getTime();
        dirInfo->dirLoaded = false;
        dirInfo->type = FILETYPE_COMMITTED;
    }

    paths["/"] = dirInfo;
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
    journalFile = tmpDir + "/journal";

    // Check if the journal is empty then delete it
    if (OriFile_Exists(journalFile) && (OriFile_GetSize(journalFile) == 0))
        OriFile_Delete(journalFile);

    // Attempt to delete the temporary directory if it exists
    if (OriFile_Exists(tmpDir) && OriFile_RmDir(tmpDir) != 0) {
        printf("\nAn error has occurred!\n");
        printf("\nProblem: orifs may have previously exited uncleanly\n\n");
        printf("Solution:\n"
    "Check the .ori/tmp/fuse directory for any files that may not have been\n"
    "saved to the file systems store.  You can copy or move these files to\n"
    "another location. Then delete the .ori/tmp/fuse directory and all\n"
    "remaining files.\n\n");
        printf("Notes: This is a known bug and will be fixed in the future.\n");
        exit(1);
    }

    if (::mkdir(tmpDir.c_str(), 0700) < 0)
        throw SystemException(errno);

    journalFd = ::creat(journalFile.c_str(), 0644);
    if (journalFd < 0)
        throw SystemException(errno);
}

void
OriPriv::init()
{
    UDSServerStart(repo);
}

int
cleanupHelper(OriPriv *priv, const string &path)
{
    OriFile_Delete(path);

    return 0;
}

void
OriPriv::cleanup()
{
    tmpDir = repo->getRootPath() + ORI_PATH_TMP + "fuse";

    // XXX: Delete all files on exit, but need support to delete only closed 
    // and committed files.  This would allow us to reclaim temporary space 
    // after a commit.
    DirIterate(tmpDir, this, cleanupHelper);

    UDSServerStop();
}

pair<string, int>
OriPriv::getTemp()
{
    string filePath = tmpDir + "/obj.XXXXXX";
    char tmpPath[PATH_MAX];
    int fd;

    strncpy(tmpPath, filePath.c_str(), PATH_MAX);

    fd = mkstemp(tmpPath);
    if (fd < 0)
        throw SystemException(errno);

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

    // Must call getDir to make sure it is loaded
    if (path != "/") {
        string parentPath = OriFile_Dirname(path);
        if (parentPath == "")
            parentPath = "/";

        getDir(parentPath);
    }

    // Check pending directories
    it = paths.find(path);
    if (it != paths.end()) {
        OriFileInfo *info = (*it).second;

        if (info->type == FILETYPE_NULL)
            throw SystemException(ENOENT);

        return info;
    }

    // Check repository

    throw SystemException(ENOENT);
}

OriFileInfo *
OriPriv::getFileInfo(uint64_t fh)
{
    unordered_map<uint64_t, OriFileInfo*>::iterator it;

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
    int status = 0;

    ASSERT(handles.find(fh) != handles.end());

    // Manage open count
    handles[fh]->releaseFd();
    if (handles[fh]->openCount == 0 && handles[fh]->fd != -1) {
        // Close file
        status = close(handles[fh]->fd);
        handles[fh]->fd = -1;
    }

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
    info->type = FILETYPE_DIRTY;
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
        it->second->release();
    }

    paths[path] = info;
    handles[handle] = info;

    info->retain();
    info->retainFd();

    return make_pair(info, handle);
}

pair<OriFileInfo *, uint64_t>
OriPriv::openFile(const string &path, bool writing, bool trunc)
{
    OriFileInfo *info = getFileInfo(path);
    uint64_t handle = generateFH();

    // XXX: Need to release and remove the hanlde during a failure!

    info->retain();
    info->retainFd();
    handles[handle] = info;

    // Open temporary file if necessary
    if (info->type == FILETYPE_DIRTY && info->path != "") {
        if (info->fd != -1) {
            // File is already open just return a new handle
            return make_pair(info, handle);
        }
        int status = open(info->path.c_str(), O_RDWR);
        if (status < 0) {
            ASSERT(false); // XXX: Need to release the handle
            throw SystemException(errno);
        }
        info->fd = status;
    } else if (info->type == FILETYPE_COMMITTED ||
               (info->type == FILETYPE_DIRTY && info->path == "")) {
        ASSERT(!info->hash.isEmpty());
        if (!writing) {
            // Read-only
            info->fd = -1;
        } else if (writing && trunc) {
            // Generate temporary file
            pair<string, int> temp = getTemp();

            info->statInfo.st_size = 0;
            info->statInfo.st_blocks = 0;
            info->type = FILETYPE_DIRTY;
            info->path = temp.first;
            info->fd = temp.second;
        } else if (writing) {
            // Copy file
            int status;
            pair<string, int> temp = getTemp();
            close(temp.second);

            if (!repo->copyObject(info->hash, temp.first)) {
                ASSERT(false); // XXX: Need to release the handle
                throw SystemException(EIO);
            }

            status = open(temp.first.c_str(), O_RDWR);
            if (status < 0) {
                ASSERT(false); // XXX: Need to release the handle
                throw SystemException(errno);
            }

            info->type = FILETYPE_DIRTY;
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
    ASSERT(!info->hash.isEmpty());

    ObjectType type = repo->getObjectType(info->hash);
    if (type == ObjectInfo::Blob) {
        string payload = repo->getPayload(info->hash);
        // XXX: Cache

        size_t left = payload.size() - offset;
        if (left > payload.size())
            left = 0;
        size_t real_read = min(size, left);

        memcpy(buf, payload.data() + offset, real_read);

        return real_read;
    } else if (type == ObjectInfo::LargeBlob) {
        LargeBlob lb = LargeBlob(repo);
        lb.fromBlob(repo->getPayload(info->hash));
        // XXX: Cache

        ssize_t total = 0;
        while (total < size) {
            ssize_t res = lb.read((uint8_t*)(buf + total),
                                 size - total,
                                 offset + total);
            if (res == 0)
                return total;
            else if (res < 0)
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
    string parentPath;
    OriDir *parentDir;

    parentPath = OriFile_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    parentDir = getDir(parentPath);

    ASSERT(info->isSymlink() || info->isReg());

    parentDir->remove(OriFile_Basename(path));
    paths.erase(path);

    // Drop refcount only delete if zero (including temp file)
    info->release();
}

void
OriPriv::rename(const string &fromPath, const string &toPath)
{
    string fromParent, toParent;
    OriDir *fromDir;
    OriDir *toDir;
    OriFileInfo *info = getFileInfo(fromPath);
    OriFileInfo *toFile = NULL;

    fromParent = OriFile_Dirname(fromPath);
    if (fromParent == "")
        fromParent = "/";
    toParent = OriFile_Dirname(toPath);
    if (toParent == "")
        toParent = "/";

    fromDir = getDir(fromParent);
    toDir = getDir(toParent);

    try {
        toFile = getFileInfo(toPath);
    } catch (SystemException &e) {
        // Fall through
    }

    // XXX: Need to rename all files under a directory
    if (info->isDir()) {
        // XXX: Handle refcount movement then we can rename empty directories
        NOT_IMPLEMENTED(!info->dirLoaded);
    }

    paths.erase(fromPath);
    paths[toPath] = info;

    string from = OriFile_Basename(fromPath);
    string to = OriFile_Basename(toPath);

    fromDir->remove(from);
    toDir->add(to, info->id);

    // Delete previously present file
    if (toFile != NULL) {
        toFile->release();
    }

    ASSERT(paths.find(fromPath) == paths.end());
    ASSERT(paths.find(toPath) != paths.end());
}

OriFileInfo *
OriPriv::addDir(const string &path)
{
    OriFileInfo *info;
    string parentPath;
    OriDir *parentDir;
    OriFileInfo *parentInfo;
    time_t now = time(NULL);

    parentPath = OriFile_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    /*
     * getDir must be called first to load the parent directory's structure 
     * into the local cache.  The following two lines may throw a 
     * SystemException if the parent doesn't exist so be careful to not 
     * allocate memory before this point.
     */
    parentDir = getDir(parentPath);
    parentInfo = getFileInfo(parentPath);

    info = new OriFileInfo();

    info->statInfo.st_uid = geteuid();
    info->statInfo.st_gid = getegid();
    /*
     * Warning: OriPriv::Merge depends on st_mode == S_IFDIR as it calls 
     * loadAttrs, which will assume the mode bits are empty except for this 
     * flag.
     */
    info->statInfo.st_mode = S_IFDIR;
    info->statInfo.st_nlink = 2;
    info->statInfo.st_size = 512;
    info->statInfo.st_atime = 0;
    info->statInfo.st_mtime = now;
    info->statInfo.st_ctime = now;
    info->type = FILETYPE_DIRTY;
    info->id = generateId();
    info->dirLoaded = true;

    dirs[info->id] = new OriDir();
    paths[path] = info;

    parentDir->add(OriFile_Basename(path), info->id);
    parentInfo->statInfo.st_nlink++;

    return info;
}

void
OriPriv::rmDir(const string &path)
{
    OriDir *dir = getDir(path);
    OriFileInfo *info = getFileInfo(path);
    string parentPath;
    OriDir *parentDir;
    OriFileInfo *parentInfo;

    parentPath = OriFile_Dirname(path);
    if (parentPath == "")
        parentPath = "/";

    parentDir = getDir(parentPath);
    parentInfo = getFileInfo(parentPath);

    /*
     * XXX: We don't handle deleteing files inside the directory so either the 
     * directory must be empty or not loaded.  While the function ori_rmdir 
     * will prevent non-empty directories from being deleted, other uses inside 
     * OriPriv need this functionality to be complete.
     */
    ASSERT(dirs[info->id]->isEmpty() || !info->dirLoaded);

    parentDir->remove(OriFile_Basename(path));
    parentInfo->statInfo.st_nlink--;
    parentInfo->type = FILETYPE_DIRTY;

    ASSERT(parentInfo->statInfo.st_nlink >= 2);

    dirs.erase(info->id);
    paths.erase(path);

    delete dir;
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
            throw SystemException(ENOTDIR);
        if ((*it).second->type == FILETYPE_NULL)
            throw SystemException(ENOENT);
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

        dirInfo = getFileInfo(path);

        for (it = t.begin(); it != t.end(); it++) {
            OriFileInfo *info = new OriFileInfo();
            AttrMap *attrs = &it->second.attrs;
            bool isSymlink = false;

            if (it->second.type == TreeEntry::Tree) {
                info->statInfo.st_mode = S_IFDIR;
                info->statInfo.st_nlink = 2;
                // XXX: This is hacky but a directory gets the correct nlink 
                // value once it is opened for the first time.
                dirInfo->statInfo.st_nlink++;
            }
            if (attrs->has(ATTR_SYMLINK)) {
                isSymlink = attrs->getAs<bool>(ATTR_SYMLINK);
            }
            info->loadAttr(*attrs);
            info->type = FILETYPE_COMMITTED;
            info->id = generateId();
            info->hash = it->second.hash;
            info->largeHash = it->second.largeHash;
            if (isSymlink) {
                ASSERT(info->largeHash.isEmpty());
                info->link = repo->getPayload(info->hash);
            }

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

    throw SystemException(ENOENT);
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
 * Command Operations
 */

ObjectHash
OriPriv::getTip()
{
    return head;
}

ObjectHash
OriPriv::commitTreeHelper(const string &path)
{
    ObjectHash hash = ObjectHash();
    OriDir *dir = getDir(path == "" ? "/" : path);
    Tree oldTree = Tree();
    Tree newTree;
    bool dirty = false;

    // Load repo directory
    try {
        /*
         * XXX: This is unclean but lookup returns an empty hash or
         * throws a runtime_error depending on a few corner cases when the 
         * directory does not exist.
         */
        ObjectHash treeHash = repo->lookup(headCommit,
                                           path == "" ? "/" : path);
        if (!treeHash.isEmpty()) {
            oldTree = repo->getTree(treeHash);
        } else {
            dirty = true;
        }
    } catch (runtime_error &e) {
        // Directory does not exist
        dirty = true;
    }

    // Check this directory
    for (OriDir::iterator it = dir->begin(); it != dir->end(); it++) {
        string objPath = path + "/" + it->first;
        OriFileInfo *info = getFileInfo(objPath);

        if (info->type == FILETYPE_DIRTY) {
            dirty = true;
        
            // Created or modified
            TreeEntry e;

            if (info->isSymlink()) {
                ObjectHash hash = repo->addBlob(ObjectInfo::Blob, info->link);

                // Copy hash back to info stgructure
                info->hash = hash;

                e = TreeEntry(hash, ObjectHash());
            } else {
                if (info->path != "") {
                    pair<ObjectHash, ObjectHash> hashes;
                    hashes = repo->addFile(info->path);

                    // Copy hashes back to info stgructure
                    info->hash = hashes.first;
                    info->largeHash = hashes.second;
                }
                e = TreeEntry(info->hash, info->largeHash);
            }

            info->storeAttr(&e.attrs);

            if (info->isDir()) {
                e.type = TreeEntry::Tree;
            } else {
                if (e.largeHash.isEmpty())
                    e.type = TreeEntry::Blob;
                else
                    e.type = TreeEntry::LargeBlob;
            }

            ASSERT(e.hasBasicAttrs());

            newTree.tree[it->first] = e;

            info->type = FILETYPE_COMMITTED;
        } else {
            Tree::iterator oldEntry = oldTree.find(it->first);

            ASSERT(oldEntry != oldTree.end());
            ASSERT(oldEntry->second.hasBasicAttrs());
            ASSERT(!oldEntry->second.hash.isEmpty());

            // Copy old entry
            newTree.tree[it->first] = oldEntry->second;
        }
    }
    for (Tree::iterator it = oldTree.begin(); it != oldTree.end(); it++) {
        string objPath = path + "/" + it->first;

        if (dir->find(it->first) == dir->end()) {
            dirty = true;
            // Deleted
        }
    }

    // Check subdirectories
    for (OriDir::iterator it = dir->begin(); it != dir->end(); it++) {
        string objPath = path + "/" + it->first;
        OriFileInfo *info = getFileInfo(objPath);

        if (info->isDir() && info->dirLoaded) {
            ObjectHash subdir = commitTreeHelper(objPath);

            if (!subdir.isEmpty()) {
                dirty = true;

                // Save new hash
                newTree.tree[it->first].hash = subdir;
            } else if (newTree.tree[it->first].hash.isEmpty()) {
                Tree::iterator oldEntry = oldTree.find(it->first);
                ASSERT(oldEntry != oldTree.end());
                newTree.tree[it->first].hash = oldEntry->second.hash;
            }
        }

        ASSERT(!newTree.tree[it->first].hash.isEmpty());
    }

    if (dirty) {
        hash = repo->addTree(newTree);
    } else {
        ASSERT(hash.isEmpty());
    }

    return hash;
}

ObjectHash
OriPriv::commit(const Commit &cTemplate, bool temporary)
{
    Commit c;
    ObjectHash root = commitTreeHelper("");
    ObjectHash commitHash = ObjectHash();

    if (root.isEmpty() || root == headCommit.getTree())
        return commitHash;

    c.setMessage(cTemplate.getMessage());
    c.setSnapshot(cTemplate.getSnapshot());
    commitHash = repo->commitFromTree(root, c);

    head = repo->getHead();
    headCommit = repo->getCommit(head);

    repo->sync();

    journal("snapshot", commitHash.hex());

    return commitHash;
}

void
OriPriv::getDiffHelper(const string &path,
                       map<string, OriFileState::StateType> *diff)
{
    OriDir *dir = getDir(path == "" ? "/" : path);
    Tree t;

    // Load repo directory
    try {
        ObjectHash treeHash = repo->lookup(headCommit,
                                           path == "" ? "/" : path);
        if (treeHash.isEmpty())
            return;

        t = repo->getTree(treeHash);
    } catch (runtime_error &e) {
        // Directory does not exist
        return;
    }

    // Check this directory
    for (OriDir::iterator it = dir->begin(); it != dir->end(); it++) {
        string objPath = path + "/" + it->first;
        OriFileInfo *info = getFileInfo(objPath);

        if (info->type == FILETYPE_DIRTY) {
            if (t.find(it->first) == t.end())
                diff->insert(make_pair(objPath, OriFileState::Created));
            else
                diff->insert(make_pair(objPath, OriFileState::Modified));
        }
    }
    for (Tree::iterator it = t.begin(); it != t.end(); it++) {
        string objPath = path + "/" + it->first;

        if (dir->find(it->first) == dir->end())
            diff->insert(make_pair(objPath, OriFileState::Deleted));
    }

    // Check subdirectories
    for (OriDir::iterator it = dir->begin(); it != dir->end(); it++) {
        string objPath = path + "/" + it->first;
        OriFileInfo *info = getFileInfo(objPath);

        if (info->isDir() && info->dirLoaded) {
            getDiffHelper(objPath, diff);
        }
    }
}

map<string, OriFileState::StateType>
OriPriv::getDiff()
{
    map<string, OriFileState::StateType> diff;

    getDiffHelper("", &diff);

    return diff;
}

void
OriPriv::getCheckoutHelper(const string &path,
                           map<string, OriFileInfo *> *diffInfo,
                           map<string, OriFileState::StateType> *diffState)
{
    OriDir *dir = getDir(path == "" ? "/" : path);
    Tree t;

    // Load repo directory
    // XXX: This function needs to return all new objects in current diff
    try {
        ObjectHash treeHash = repo->lookup(headCommit,
                                           path == "" ? "/" : path);
        if (treeHash.isEmpty())
            return;

        t = repo->getTree(treeHash);
    } catch (runtime_error &e) {
        // Directory does not exist
        return;
    }

    // Check this directory
    for (OriDir::iterator it = dir->begin(); it != dir->end(); it++) {
        string objPath = path + "/" + it->first;
        OriFileInfo *info = getFileInfo(objPath);

        if (info->type == FILETYPE_DIRTY) {
            if (t.find(it->first) == t.end()) {
                diffState->insert(make_pair(objPath, OriFileState::Created));
                info->retain();
                diffInfo->insert(make_pair(objPath, info));
            } else {
                diffState->insert(make_pair(objPath, OriFileState::Modified));
                info->retain();
                diffInfo->insert(make_pair(objPath, info));
            }
        }
    }
    for (Tree::iterator it = t.begin(); it != t.end(); it++) {
        string objPath = path + "/" + it->first;

        if (dir->find(it->first) == dir->end()) {
            diffState->insert(make_pair(objPath, OriFileState::Deleted));
        }
    }

    // Check subdirectories
    for (OriDir::iterator it = dir->begin(); it != dir->end(); it++) {
        string objPath = path + "/" + it->first;
        OriFileInfo *info = getFileInfo(objPath);

        if (info->isDir() && info->dirLoaded) {
            getCheckoutHelper(objPath, diffInfo, diffState);
        }
    }
}

string
OriPriv::checkout(ObjectHash hash, bool force)
{
    Commit c = repo->getCommit(hash);
    map<string, OriFileInfo *> diffInfo;
    map<string, OriFileState::StateType> diffState;

    getCheckoutHelper("", &diffInfo, &diffState);

    // Store a set of directories containing changes
    map<string, OriFileState::StateType>::iterator it;
    set<string> modifiedDirs;
    for (it = diffState.begin(); it != diffState.end(); it++) {
        string base = OriFile_Dirname(it->first);

        if (base == "")
            base = "/";

        modifiedDirs.insert(base);
    }

    // Reset
    map<string, OriFileInfo*>::iterator pit;
    map<OriPrivId, OriDir*>::iterator dit;
    for (pit = paths.begin(); pit != paths.end(); pit++)
    {
        if (pit->first == "/") {
            if (pit->second->dirLoaded)
                dirs.erase(pit->second->id);
            pit->second->statInfo.st_nlink = 2;
            pit->second->dirLoaded = false;
            continue;
        }

        if (pit->second->isDir() && pit->second->dirLoaded) {
            delete dirs[pit->second->id];
            dirs.erase(pit->second->id);
        }
        pit->second->release();
        paths.erase(pit);
    }

    OriFileInfo *rootInfo = paths["/"];
    rootInfo->statInfo.st_mtime = c.getTime();
    rootInfo->statInfo.st_ctime = c.getTime();

    if (force) {
        // Drop reference counts
        for (pit = diffInfo.begin(); pit != diffInfo.end(); pit++) {
            pit->second->release();
        }

        // Update head
        head = hash;
        headCommit = c;
        repo->updateHead(head);
        return "";
    }

    // Compute diff to detect conflicts

    // getDir for current dirs
    set<string>::iterator mdit;
    for (mdit = modifiedDirs.begin(); mdit != modifiedDirs.end(); mdit++) {
        getDir(*mdit);
    }

    // Merge files (unless force specified)
    for (it = diffState.begin(); it != diffState.end(); it++) {
        string filePath = it->first;
        string parentPath = OriFile_Dirname(filePath);

        if (parentPath == "")
            parentPath = "/";

        switch (it->second) {
            case OriFileState::Invalid:
                NOT_IMPLEMENTED(false);
                break;
            case OriFileState::Created:
            {
                OriFileInfo *info = diffInfo[filePath];
                OriDir *parentDir = getDir(parentPath);

                // Rename conflicting file if it exists
                if (paths.find(filePath) != paths.end()) {
                    rename(filePath, filePath + ":create_conflict");
                }

                // Create the new file
                paths[it->first] = info;
                parentDir->add(OriFile_Basename(filePath), info->id);
                if (info->isDir()) {
                    OriFileInfo *parentInfo = getFileInfo(parentPath);
                    parentInfo->statInfo.st_nlink++;

                    dirs[info->id] = new OriDir();
                }
                break;
            }
            case OriFileState::Deleted:
            {
                OriFileInfo *info = getFileInfo(filePath);
                if (info != NULL) {
                    if (info->isDir()) {
                        rmDir(filePath);
                    } else {
                        unlink(filePath);
                    }
                }
                break;
            }
            case OriFileState::Modified:
            {
                OriFileInfo *newInfo = getFileInfo(filePath);
                OriFileInfo *myInfo = diffInfo[filePath];
                OriDir *parentDir = getDir(parentPath);

                if (newInfo == NULL) {
                    // File was deleted
                    myInfo->release();
                } else if (newInfo->hash != myInfo->hash) {
                    // Conflict
                    rename(filePath, filePath + ":conflict");
                    parentDir->add(OriFile_Basename(filePath), myInfo->id);
                } else {
                    // No conflict
                    paths[it->first] = myInfo;
                    parentDir->add(OriFile_Basename(filePath), myInfo->id);
                    newInfo->release();
                }
                break;
            }
            default:
                NOT_IMPLEMENTED(false);
                break;
        }
    }

    // Update head
    head = hash;
    headCommit = c;
    repo->updateHead(head);

    return "";
}

string
OriPriv::merge(ObjectHash hash)
{
    ObjectHash p1 = head;
    ObjectHash p2 = hash;
    DAG<ObjectHash, Commit> cDag = repo->getCommitDag();
    ObjectHash lca;

    lca = cDag.findLCA(p1, p2);

    Commit c1 = headCommit;
    Commit c2 = repo->getCommit(p2);

    Tree t1 = repo->getTree(c1.getTree());
    Tree t2 = repo->getTree(c2.getTree());
    Tree tc;

    if (lca != EMPTY_COMMIT) {
        Commit cc = repo->getCommit(lca);
        tc = repo->getTree(cc.getTree());
    }

    // Load flattened trees
    TreeDiff td1, td2;
    Tree::Flat t1Flat = t1.flattened(repo);
    Tree::Flat t2Flat = t2.flattened(repo);
    Tree::Flat tcFlat = tc.flattened(repo);

    // Apply current changes to t1Flat
    map<string, OriFileInfo *> diffInfo;
    map<string, OriFileState::StateType> diffState;
    map<string, OriFileState::StateType>::iterator dsit;

    getCheckoutHelper("", &diffInfo, &diffState);

    for (dsit = diffState.begin(); dsit != diffState.end(); dsit++) {
        // Update tree
        switch (dsit->second) {
            case OriFileState::Invalid:
            {
                NOT_IMPLEMENTED(false);
                break;
            }
            case OriFileState::Created:
            {
                // XXX: Need to rewrite the flat tree and merge code, but
                // for now we zero out the object hash for new files.
                OriFileInfo *info = diffInfo[dsit->first];
                TreeEntry te = TreeEntry(info->hash, info->largeHash);
                info->storeAttr(&te.attrs);
                if (info->isDir()) {
                    te.type = TreeEntry::Tree;
                } else {
                    if (te.largeHash.isEmpty())
                        te.type = TreeEntry::Blob;
                    else
                        te.type = TreeEntry::LargeBlob;
                }
                ASSERT(te.hasBasicAttrs());
                t1Flat[dsit->first] = te;
                break;
            }
            case OriFileState::Deleted:
            {
                t1Flat.erase(dsit->first);
                break;
            }
            case OriFileState::Modified:
            {
                // XXX: Need to rewrite the flat tree and merge code, but
                // for now we zero out the object hash for modified files.
                OriFileInfo *info = diffInfo[dsit->first];
                TreeEntry te = t1Flat[dsit->first];
                te.hash = ObjectHash();
                info->storeAttr(&te.attrs);
                te.hasBasicAttrs();
                t1Flat[dsit->first] = te;
                break;
            }
            default:
            {
                NOT_IMPLEMENTED(false);
                break;
            }
        }
    }

    // Create diffs and compute merge
    td1.diffTwoTrees(t1Flat, tcFlat);
    LOG("Diff from %s to %s", lca.hex().c_str(), p1.hex().c_str());
    td1.dump();
    td2.diffTwoTrees(t2Flat, tcFlat);
    LOG("Diff from %s to %s", lca.hex().c_str(), p2.hex().c_str());
    td2.dump();

    // XXX: Need to handle merging file attributes
    TreeDiff mdiff;
    mdiff.mergeTrees(td1, td2);
    LOG("Merged diffs");
    mdiff.dump();

    // Recompute the merge relative to the working tree
    TreeDiff wdiff;
    wdiff.mergeChanges(td1, mdiff);
    LOG("Result of MergeChanges");
    wdiff.dump();

    // Slower way to recompute merge relative to working tree
    //TreeDiff rdiff;
    //mdiff.applyTo(&tcFlat);
    //rdiff.diffTwoTrees(tcFlat, t1Flat);
    //LOG("Slow recomputed diff");
    //rdiff.dump();

    MergeState state;
    state.setParents(p1, p2);

    DLOG("Applying merge:");
    for (size_t i = 0; i < wdiff.entries.size(); i++) {
        TreeDiffEntry e = wdiff.entries[i];

        if (e.type == TreeDiffEntry::NewFile) {
            DLOG("N       %s", e.filepath.c_str());
            OriFileInfo *info = new OriFileInfo();
            bool isSymlink = false;

            if (e.newAttrs.has(ATTR_SYMLINK)) {
                isSymlink = e.newAttrs.getAs<bool>(ATTR_SYMLINK);
            }
            info->loadAttr(e.newAttrs);
            info->id = generateId();
            info->hash = e.hashes.first;
            info->largeHash = e.hashes.second;
            info->type = FILETYPE_DIRTY;
            if (isSymlink) {
                ASSERT(info->largeHash.isEmpty());
                info->link = repo->getPayload(info->hash);
            }

            OriDir *parentDir = getDir(OriFile_Dirname(e.filepath));
            parentDir->add(OriFile_Basename(e.filepath), info->id);
            paths[e.filepath] = info;
        } else if (e.type == TreeDiffEntry::NewDir) {
            DLOG("N       %s", e.filepath.c_str());
            OriFileInfo *info = addDir(e.filepath);
            info->loadAttr(e.newAttrs);
            info->statInfo.st_nlink++;
            info->hash = e.hashes.first;
            info->largeHash = e.hashes.second;
            info->type = FILETYPE_DIRTY;
        } else if (e.type == TreeDiffEntry::DeletedFile) {
            DLOG("D       %s", e.filepath.c_str());
            unlink(e.filepath);
        } else if (e.type == TreeDiffEntry::DeletedDir) {
            DLOG("D       %s", e.filepath.c_str());
            rmDir(e.filepath); 
        } else if (e.type == TreeDiffEntry::Modified) {
            DLOG("U       %s", e.filepath.c_str());
            // Calling getDir ensures that the fileinfo is loaded
            getDir(OriFile_Dirname(e.filepath));
            OriFileInfo *info = getFileInfo(e.filepath);

            if (info->path != "") {
                ASSERT(info->type == FILETYPE_COMMITTED);
                int status = OriFile_Delete(info->path);
                if (status < 0) {
                    WARNING("Failed to unlink temporary file '%s': %s",
                            info->path.c_str(), Util_SystemError(status).c_str());
                }
                info->path = "";
            }

            info->hash = e.hashes.first;
            info->largeHash = e.hashes.second;
            info->loadAttr(e.newAttrs);
            info->type = FILETYPE_DIRTY;
        } else if (e.type == TreeDiffEntry::MergeConflict) {
            DLOG("X       %s (CONFLICT)", e.filepath.c_str());
            bool mergeSuccess = false;
            OriDir *parentDir = getDir(OriFile_Dirname(e.filepath));
            OriFileInfo *info = getFileInfo(e.filepath);

            ASSERT(info != NULL);

            if (info->path != "") {
                // XXX: Attempt automerge
            }

            if (!mergeSuccess) {
                // Create '*:conflict' file
                OriFileInfo *conflictInfo = new OriFileInfo();
                bool isSymlink = false;
                
                ASSERT(!e.hashB.first.isEmpty());
                if (e.newAttrs.has(ATTR_SYMLINK)) {
                    isSymlink = e.newAttrs.getAs<bool>(ATTR_SYMLINK);
                }
                conflictInfo->loadAttr(e.attrsB);
                conflictInfo->id = generateId();
                conflictInfo->hash = e.hashB.first;
                conflictInfo->largeHash = e.hashB.second;
                conflictInfo->type = FILETYPE_DIRTY;
                if (isSymlink) {
                    ASSERT(conflictInfo->largeHash.isEmpty());
                    conflictInfo->link = repo->getPayload(conflictInfo->hash);
                }

                parentDir->add(OriFile_Basename(e.filepath) + ":conflict",
                               conflictInfo->id);
                paths[e.filepath + ":conflict"] = conflictInfo;

                /*
                 * Create '*:base' file if it exists.  It may not exist because 
                 * it is possible that two files were created with the same 
                 * name.
                 */
                if (!e.hashBase.first.isEmpty()) {
                    OriFileInfo *baseInfo = new OriFileInfo();
                    isSymlink = false;

                    if (e.newAttrs.has(ATTR_SYMLINK)) {
                        isSymlink = e.newAttrs.getAs<bool>(ATTR_SYMLINK);
                    }
                    baseInfo->loadAttr(e.attrsBase);
                    baseInfo->id = generateId();
                    baseInfo->hash = e.hashBase.first;
                    baseInfo->largeHash = e.hashBase.second;
                    baseInfo->type = FILETYPE_DIRTY;
                    if (isSymlink) {
                        ASSERT(baseInfo->largeHash.isEmpty());
                        baseInfo->link = repo->getPayload(baseInfo->hash);
                    }

                    parentDir->add(OriFile_Basename(e.filepath) + ":base",
                                   baseInfo->id);
                    paths[e.filepath + ":base"] = baseInfo;
                }
            }

            /*int status;
            Blob pivot, a, b, out;
            string path;
            string pivotStr;
            string aStr;
            string bStr;
            
            path = repository.getRootPath() + mdiff.entries[i].filepath;
           
            if (mdiff.entries[i].hashBase.first == EMPTYFILE_HASH) {
                pivotStr = "";
            } else {
                pivotStr = repository.getPayload(mdiff.entries[i].hashBase.first);
            }
            aStr = repository.getPayload(mdiff.entries[i].hashA.first);
            bStr = repository.getPayload(mdiff.entries[i].hashB.first);

            blob_init(&pivot, pivotStr.data(), pivotStr.size());
            blob_init(&a, aStr.data(), aStr.size());
            blob_init(&b, bStr.data(), bStr.size());
            blob_zero(&out);
            status = blob_merge(&pivot, &a, &b, &out);
            if (status == 0) {
                printf("U       %s (MERGED)\n", e.filepath.c_str());
                blob_write_to_file(&out, path.c_str());
            } else {
                printf("X       %s (CONFLICT)\n", e.filepath.c_str());
                repository.copyObject(mdiff.entries[i].hashBase.first,
                                      path + ".base");
                repository.copyObject(mdiff.entries[i].hashA.first,
                                      path + ".yours");
                repository.copyObject(mdiff.entries[i].hashB.first,
                                      path + ".theirs");
                conflictExists = true;
                // XXX: Allow optional call to external diff tool
            }*/
        } else if (e.type == TreeDiffEntry::FileDirConflict) {
            DLOG("X       %s (DIR-FILE CONFLICT)", e.filepath.c_str());
            NOT_IMPLEMENTED(false);
        } else {
            DLOG("Unsupported TreeDiffEntry type %c", e.type);
            NOT_IMPLEMENTED(false);
        }
    }

    // XXX: Update the hashes for any not loaded directories.

    // XXX: Need to force a snapshot

    return "";
}

void
OriPriv::setJournalMode(OriJournalMode::JournalMode mode)
{
    journalMode = mode;
}

void
OriPriv::journal(const string &event, const string &arg)
{
    int len;
    string buf;

    if (journalMode == OriJournalMode::NoJournal)
        return;

    buf = event + ":" + arg + "\n";
    len = write(journalFd, buf.c_str(), buf.size());
    if (len < 0 || len != (int)buf.size())
        throw SystemException();

    if (journalMode == OriJournalMode::SyncJournal)
        fsync(journalFd);

    return;
}

/*
 * Debugging
 */

void
OriPrivCheckDir(OriPriv *priv, const string &path, OriDir *dir)
{
    OriDir::iterator it;

    for (it = dir->begin(); it != dir->end(); it++) {
        string objPath = path + "/" + it->first;
        OriFileInfo *info = NULL;

        try {
            info = priv->getFileInfo(objPath);
        } catch (SystemException e) {
            FUSE_LOG("fsck: getFileInfo(%s) had %s",
                     objPath.c_str(), e.what());
        }

        if (info && info->isDir() && info->dirLoaded) {
            OriDir *dir;

            ASSERT(!info->isSymlink() && !info->isReg());

            try {
                dir = priv->getDir(objPath);
                OriPrivCheckDir(priv, objPath, dir);
            } catch (SystemException e) {
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

void
OriPriv::fsck()
{
    RWKey::sp lock;
    map<string, OriFileInfo *>::iterator it;
    OriDir *dir;

    lock = nsLock.writeLock();

    dir = getDir("/");

    OriPrivCheckDir(this, "", dir);

    for (it = paths.begin(); it != paths.end(); it++) {
        string basename = OriFile_Basename(it->first);
        string parentPath = OriFile_Dirname(it->first);
        OriDir *dir = NULL;

        if (it->first == "/")
            continue;

        if (parentPath == "")
            parentPath = "/";

        try {
            dir = getDir(parentPath);
        } catch (SystemException e) {
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

LocalRepo *
OriPriv::getRepo()
{
    return repo;
}

OriPriv *
GetOriPriv()
{
    return (OriPriv*)fuse_get_context()->private_data;
}

