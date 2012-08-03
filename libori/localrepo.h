#ifndef __LOCALREPO_H__
#define __LOCALREPO_H__

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

#define ORI_PATH_DIR "/.ori"
#define ORI_PATH_VERSION "/.ori/version"
#define ORI_PATH_UUID "/.ori/id"
#define ORI_PATH_INDEX "/.ori/index"
#define ORI_PATH_SNAPSHOTS "/.ori/snapshots"
#define ORI_PATH_METADATA "/.ori/metadata"
#define ORI_PATH_DIRSTATE "/.ori/dirstate"
#define ORI_PATH_BRANCH "/.ori/BRANCH"
#define ORI_PATH_LOG "/.ori/ori.log"
#define ORI_PATH_TMP "/.ori/tmp/"
#define ORI_PATH_OBJS "/.ori/objs/"
#define ORI_PATH_HEADS "/.ori/refs/heads/"
#define ORI_PATH_REMOTES "/.ori/refs/remotes/"
#define ORI_PATH_LOCK "/.ori/lock"

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
    typedef std::auto_ptr<LocalRepoLock> ap;
};

class LocalRepo : public Repo
{
public:
    LocalRepo(const std::string &root = "");
    ~LocalRepo();
    bool open(const std::string &root = "");
    void close();
    void save();
    LocalRepoLock *lock();

    // Remote Repository (Thin/Insta-clone)
    void setRemote(Repo *r);
    void clearRemote();
    bool hasRemote();

    // Object Operations
    int addObjectRaw(const ObjectInfo &info,
            bytestream *bs);
    bool hasObject(const ObjectHash &objId);
    Object::sp getObject(const ObjectHash &objId);
    ObjectInfo getObjectInfo(const ObjectHash &objId);
    std::set<ObjectInfo> slowListObjects();
    std::set<ObjectInfo> listObjects();
    bool rebuildIndex();

    LocalObject::sp getLocalObject(const ObjectHash &objId);
    
    std::vector<Commit> listCommits();
    std::map<std::string, std::string> listSnapshots();

    ObjectHash addTree(const Tree &tree);
    ObjectHash addCommit(/* const */ Commit &commit);
    //std::string addBlob(const std::string &blob, Object::Type type);
    size_t getObjectLength(const ObjectHash &objId);
    Object::Type getObjectType(const ObjectHash &objId);
    std::string getPayload(const ObjectHash &objId);
    std::string verifyObject(const ObjectHash &objId);
    bool purgeObject(const ObjectHash &objId);
    size_t sendObject(const char *objId);

    // Repository Operations
    bool copyObject(const ObjectHash &objId, const std::string &path);

    // Clone/pull operations
    void pull(Repo *r);

    // Commit-related operations
    void addLargeBlobBackrefs(const LargeBlob &lb, MdTransaction::sp tr);
    void addTreeBackrefs(const Tree &lb, MdTransaction::sp tr);
    void addCommitBackrefs(const Commit &lb, MdTransaction::sp tr);

    void copyObjectsFromLargeBlob(Repo *other, const LargeBlob &lb);
    void copyObjectsFromTree(Repo *other, const Tree &t);

    /// @returns commit id
    ObjectHash commitFromTree(const ObjectHash &treeHash, Commit &c);
    ObjectHash commitFromObjects(const ObjectHash &treeHash, Repo *objects,
            Commit &c);

    void gc();

    // Reference Counting Operations
    const MetadataLog &getMetadata() const;
    RefcountMap recomputeRefCounts();
    bool rewriteRefCounts(const RefcountMap &refs);
    
    // Pruning Operations
    // void pruneObject(const std::string &objId);

    // Grafting Operations
    std::set<ObjectHash> getSubtreeObjects(const ObjectHash &treeId);
    std::set<ObjectHash> walkHistory(HistoryCB &cb);
    TreeEntry lookupTreeEntry(const Commit &c, const std::string &path);
    ObjectHash lookup(const Commit &c, const std::string &path);
    std::string graftSubtree(LocalRepo *r,
                             const std::string &srcPath,
                             const std::string &dstPath);

    // Working Directory Operations
    std::set<std::string> listBranches();
    std::string getBranch();
    void setBranch(const std::string &name);
    ObjectHash getHead();
    void updateHead(const ObjectHash &commitId);
    Tree getHeadTree(); /// @returns empty tree if HEAD is empty commit

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

    // Remote Operations
    Repo *remoteRepo;

    // Caches
    LRUCache<ObjectHash, ObjectInfo, 128> _objectInfoCache;
    // TODO: ulimit is 256 files open on OSX, need to support 2 repos open at a
    // time?
    LRUCache<ObjectHash, LocalObject::sp, 96> _objectCache;
    // Friends
    friend int LocalRepo_PeerHelper(void *arg, const char *path);
};

#endif
