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

#ifndef __LOCALREPO_H__
#define __LOCALREPO_H__

#include <tr1/memory>

#include "repo.h"
#include "index.h"
#include "snapshotindex.h"
#include "peer.h"
#include "metadatalog.h"
#include "lrucache.h"
#include "localobject.h"
#include "treediff.h"
#include "tempdir.h"
#include "largeblob.h"
#include "remoterepo.h"
#include "packfile.h"
#include "mergestate.h"
#include "key.h"

#define ORI_PATH_DIR "/.ori"
#define ORI_PATH_VERSION "/.ori/version"
#define ORI_PATH_UUID "/.ori/id"
#define ORI_PATH_INDEX "/.ori/index"
#define ORI_PATH_SNAPSHOTS "/.ori/snapshots"
#define ORI_PATH_METADATA "/.ori/metadata"
#define ORI_PATH_DIRSTATE "/.ori/dirstate"
#define ORI_PATH_HEAD "/.ori/HEAD"
#define ORI_PATH_MERGESTATE "/.ori/mergestate"
#define ORI_PATH_LOG "/.ori/ori.log"
#define ORI_PATH_TMP "/.ori/tmp/"
#define ORI_PATH_OBJS "/.ori/objs/"
#define ORI_PATH_HEADS "/.ori/refs/heads/"
#define ORI_PATH_REMOTES "/.ori/refs/remotes/"
#define ORI_PATH_PRIVATEKEY "/.ori/private.pem"
#define ORI_PATH_TRUSTED "/.ori/trusted/"
#define ORI_PATH_LOCK "/.ori/lock"
#define ORI_PATH_BACKUP_CONF "/.ori/backup.conf"

int LocalRepo_Init(const std::string &path);

class HistoryCB
{
public:
    virtual ~HistoryCB() { };
    virtual ObjectHash cb(const ObjectHash &commitId, Commit *c) = 0;
};

class LocalRepoLock
{
    std::string lockFile;
public:
    LocalRepoLock(const std::string &filename);
    ~LocalRepoLock();
    typedef std::tr1::shared_ptr<LocalRepoLock> sp;
};

class LocalRepo : public Repo
{
public:
    LocalRepo(const std::string &root = "");
    ~LocalRepo();
    bool open(const std::string &root = "");
    void close();
    void save();
    LocalRepoLock::sp lock();

    // Remote Repository (Thin/Insta-clone)
    void setRemote(Repo *r);
    void clearRemote();
    bool hasRemote();

    // Repo implementation
    int distance() { return 0; }
    Object::sp getObject(const ObjectHash &id);
    ObjectInfo getObjectInfo(const ObjectHash &objId);
    bool hasObject(const ObjectHash &objId);
    bool isObjectStored(const ObjectHash &objId);
    //std::set<ObjectInfo> slowListObjects();
    std::set<ObjectInfo> listObjects();
    int addObject(ObjectType type, const ObjectHash &hash,
            const std::string &payload);

    void sync(); /// sync all changes to disk

    // Index
    bool rebuildIndex();
    void dumpIndex();
    void dumpPackfile(packid_t packfileId);

    LocalObject::sp getLocalObject(const ObjectHash &objId);
    
    std::vector<Commit> listCommits();
    std::map<std::string, ObjectHash> listSnapshots();
    ObjectHash lookupSnapshot(const std::string &name);

    ObjectHash addTree(const Tree &tree);
    ObjectHash addCommit(/* const */ Commit &commit);
    //std::string addBlob(const std::string &blob, ObjectType type);
    size_t getObjectLength(const ObjectHash &objId);
    ObjectType getObjectType(const ObjectHash &objId);
    std::string getPayload(const ObjectHash &objId);
    std::string verifyObject(const ObjectHash &objId);
    size_t sendObject(const char *objId);

    // Repository Operations
    bool copyObject(const ObjectHash &objId, const std::string &path);

    // Clone/pull operations
    void pull(Repo *r);
    void multiPull(RemoteRepo::sp defaultRemote);
    void transmit(bytewstream *bs, const std::vector<ObjectHash> &objs);
    void receive(bytestream *bs);
    bytestream *getObjects(const std::vector<ObjectHash> &objs);

    // Commit-related operations
    void addLargeBlobBackrefs(const LargeBlob &lb, MdTransaction::sp tr);
    void addTreeBackrefs(const Tree &lb, MdTransaction::sp tr);
    void addCommitBackrefs(const Commit &lb, MdTransaction::sp tr);

    void copyObjectsFromLargeBlob(Repo *other, const LargeBlob &lb);
    void copyObjectsFromTree(Repo *other, const Tree &t);

    /// @returns commit id
    ObjectHash commitFromTree(const ObjectHash &treeHash, Commit &c,
            const std::string &status="normal");
    ObjectHash commitFromObjects(const ObjectHash &treeHash, Repo *objects,
            Commit &c, const std::string &status="normal");

    void gc();

    // Reference Counting Operations
    MetadataLog &getMetadata();
    RefcountMap recomputeRefCounts();
    bool rewriteRefCounts(const RefcountMap &refs);
    
    // Purging Operations
    bool purgeObject(const ObjectHash &objId);
    void decrefLB(const ObjectHash &lbhash, MdTransaction::sp tr);
    void decrefTree(const ObjectHash &thash, MdTransaction::sp tr);
    bool purgeCommit(const ObjectHash &commitId);
    void purgeFuseCommits();

    // Grafting Operations
    std::set<ObjectHash> getSubtreeObjects(const ObjectHash &treeId);
    std::set<ObjectHash> walkHistory(HistoryCB &cb);
    TreeEntry lookupTreeEntry(const Commit &c, const std::string &path);
    ObjectHash lookup(const Commit &c, const std::string &path);
    ObjectHash graftSubtree(LocalRepo *r,
                            const std::string &srcPath,
                            const std::string &dstPath);

    // Working Directory Operations
    std::set<std::string> listBranches();
    std::string getBranch();
    void setBranch(const std::string &name);
    ObjectHash getHead();
    void updateHead(const ObjectHash &commitId);
    void setHead(const ObjectHash &commitId);
    Tree getHeadTree(); /// @returns empty tree if HEAD is empty commit
    void setMergeState(const MergeState &state);
    MergeState getMergeState();
    void clearMergeState();
    bool hasMergeState();

    // General Operations
    TempDir::sp newTempDir();
    std::string getRootPath();
    std::string getLogPath();
    std::string getTmpFile();
    std::string getUUID();
    std::string getVersion();

    // Peer Management
    std::map<std::string, Peer> getPeers();
    bool addPeer(const std::string &name, const std::string &path);
    bool removePeer(const std::string &name);
    void setInstaClone(const std::string &name, bool val = true);

    // Key Management
    PrivateKey getPrivateKey();
    std::map<std::string, PublicKey> getPublicKeys();

    // Static Operations
    static std::string findRootPath(const std::string &path = "");
private:
    // Helper Functions
    void createObjDirs(const ObjectHash &objId);
public: // Hack to enable rebuild operations
    std::string objIdToPath(const ObjectHash &objId);
private:
    // Variables
    bool opened;
    std::string rootPath;
    std::string id;
    std::string version;
    Index index;
    SnapshotIndex snapshots;
    std::map<std::string, Peer> peers;
    MetadataLog metadata;

    // Packfiles
    Packfile::sp currPackfile;
    PfTransaction::sp currTransaction;
    PackfileManager::sp packfiles;

    // Purging
    std::set<ObjectHash> purged;

    // Repo lock
    LocalRepoLock::sp repoProcessLock;

    // Remote Operations
    Repo *remoteRepo;
    RemoteRepo resumeRepo;

    // Caches
    LRUCache<ObjectHash, ObjectInfo, 128> _objectInfoCache;
    // Friends
    friend int LocalRepo_PeerHelper(LocalRepo *l, const std::string &path);
};

#endif
