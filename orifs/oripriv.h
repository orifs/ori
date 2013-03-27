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

#ifndef __ORIPRIV_H__
#define __ORIPRIV_H__

class OriFile
{
    OriFile() { }
    ~OriFile() { }
};

typedef enum OriFileType
{
    FILETYPE_NULL,
    FILETYPE_COMMITTED,
    FILETYPE_TEMPORARY,
    FILETYPE_REMOTE,
} OriFileType;

#define ORIPRIVID_INVALID 0
typedef uint64_t OriPrivId;

class OriFileInfo
{
public:
    OriFileInfo() {
        memset(&statInfo, 0, sizeof(statInfo));
        type = FILETYPE_NULL;
        id = 0;
        path = "";
        fd = -1;
        refCount = 1;
        openCount = 0;
        dirLoaded = false;
    }
    ~OriFileInfo() {
        ASSERT(refCount == 0);
        ASSERT(openCount == 0);
    }
    /*
     * Tracks the number of handles to a file if this drops to zero the file 
     * should be deleted along with this object.  This helps to handle the case
     * that unlink is called while a file is still open.
     */
    void retain() {
        ASSERT(refCount != 0);
        refCount++;
    }
    void release() {
        refCount--;
        if (refCount == 0) {
            delete this;
        }
    }
    /*
     * Tracks the number of open handles to a file so that we can close the 
     * temporary file when there are no file descriptors left.
     */
    void retainFd() {
        openCount++;
    }
    void releaseFd() {
        openCount--;
        ASSERT(openCount >= 0);
    }
    bool isDir() { return (statInfo.st_mode & S_IFDIR) == S_IFDIR; }
    bool isSymlink() { return (statInfo.st_mode & S_IFLNK) == S_IFLNK; }
    bool isReg() { return (statInfo.st_mode & S_IFREG) == S_IFREG; }
    struct stat statInfo;
    ObjectHash hash;
    ObjectHash largeHash;
    OriFileType type;
    OriPrivId id;
    std::string path; // link target or temporary file
    int fd;
    int refCount;
    int openCount;
    bool dirLoaded;
};

class OriDir
{
public:
    typedef std::map<std::string, OriPrivId>::iterator iterator;
    OriDir() { }
    ~OriDir() { }
    void add(const std::string &name, OriPrivId id)
    {
        entries[name] = id;
        setDirty();
    }
    void remove(const std::string &name)
    {
        ASSERT(entries.find(name) != entries.end());
        entries.erase(name);
        setDirty();
    }
    bool isEmpty() { return entries.size() == 0; }
    void setDirty() { dirty = true; }
    void clrDirty() { dirty = true; }
    bool isDirty() { return dirty; }
    iterator begin() { return entries.begin(); }
    iterator end() { return entries.end(); }
    iterator find(const std::string &name) { return entries.find(name); }
private:
    bool dirty;
    std::map<std::string, OriPrivId> entries;
};

class OriFileState
{
public:
    enum StateType {
        Invalid,
        Created,
        Deleted,
        Modified,
    };
};

class OriPriv
{
public:
    OriPriv(const std::string &repoPath);
    ~OriPriv();
    // Repository Operations
    void reset();
    void cleanup();
    void setInstaClone(const std::string &origin, Repo *remoteRepo);
    std::pair<std::string, int> getTemp();
    // Current Change Operations
    uint64_t generateFH();
    OriPrivId generateId();
    OriFileInfo* getFileInfo(const std::string &path);
    OriFileInfo* getFileInfo(uint64_t fh);
    int closeFH(uint64_t fh);
    OriFileInfo* createInfo();
    OriFileInfo* addSymlink(const std::string &path);
    std::pair<OriFileInfo*, uint64_t> addFile(const std::string &path);
    std::pair<OriFileInfo*, uint64_t> openFile(const std::string &path,
                                               bool writing, bool trunc);
    size_t readFile(OriFileInfo *info, char *buf, size_t size, off_t offset);
    void unlink(const std::string &path);
    void rename(const std::string &fromPath, const std::string &toPath);
    OriFileInfo* addDir(const std::string &path);
    void rmDir(const std::string &path);
    OriDir* getDir(const std::string &path);
    // Snapshot Operations
    std::map<std::string, ObjectHash> listSnapshots();
    Commit lookupSnapshot(const std::string &name);
    Tree getTree(const Commit &c, const std::string &path);
    // Command Operations
    OriCommand cmd;
    ObjectHash getTip();
private:
    ObjectHash commitTreeHelper(const std::string &path);
    void getDiffHelper(const std::string &path,
                       std::map<std::string, OriFileState::StateType> *diff);
public:
    std::string commit(const std::string &msg, bool temporary = false);
    std::map<std::string, OriFileState::StateType> getDiff();
    // Debugging
    void fsck(bool fromCmd = false);
private:
    OriPrivId nextId;
    uint64_t nextFH;
    std::map<OriPrivId, OriDir*> dirs;
    std::map<std::string, OriFileInfo*> paths;
    std::tr1::unordered_map<uint64_t, OriFileInfo*> handles;

    // Repository State
    LocalRepo *repo;
    ObjectHash head;
    Commit headCommit;
    std::string tmpDir;

    // Locks
    RWLock ioLock; // File I/O lock to allow atomic commits
    RWLock nsLock; // Namespace lock
    RWLock cmdLock; // Control device lock

    friend class OriCommand;
};

OriPriv *GetOriPriv();

#endif /* __ORIPRIV_H__ */

