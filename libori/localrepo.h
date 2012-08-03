#ifndef __LOCALREPO_H__
#define __LOCALREPO_H__

#include "repo.h"
#include "index.h"
#include "snapshotindex.h"
#include "peer.h"
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
    virtual std::string cb(const std::string &commitId, Commit *c) = 0;
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
    bool hasObject(const std::string &objId);
    Object::sp getObject(const std::string &objId);
    ObjectInfo getObjectInfo(const std::string &objId);
    std::set<ObjectInfo> slowListObjects();
    std::set<ObjectInfo> listObjects();
    bool rebuildIndex();

    LocalObject::sp getLocalObject(const std::string &objId);
    
    std::vector<Commit> listCommits();
    std::map<std::string, std::string> listSnapshots();

    std::string addTree(const Tree &tree);
    std::string addCommit(/* const */ Commit &commit);
    //std::string addBlob(const std::string &blob, Object::Type type);
    size_t getObjectLength(const std::string &objId);
    Object::Type getObjectType(const std::string &objId);
    std::string getPayload(const std::string &objId);
    std::string verifyObject(const std::string &objId);
    bool purgeObject(const std::string &objId);
    size_t sendObject(const char *objId);
    bool copyObject(const std::string &objId, const std::string &path);
    void addBackref(const std::string &referer, const std::string &refers_to);

    // Clone/pull operations
    void pull(Repo *r);

    // Repository Operations
    void copyObjectsFromLargeBlob(Repo *other, const LargeBlob &lb);
    void copyObjectsFromTree(Repo *other, const Tree &t);
    std::string commitFromTree(const std::string &treeHash, const std::string &msg);
    std::string commitFromObjects(const std::string &treeHash, Repo *objects, const std::string &msg);
    void gc();

    // Reference Counting Operations
    std::map<std::string, Object::BRState> getRefs(const std::string &objId);
    std::map<std::string, std::map<std::string, Object::BRState> >
        getRefCounts();

    typedef std::map<std::string, std::set<std::string> > ObjReferenceMap;
    ObjReferenceMap computeRefCounts();
    bool rewriteReferences(const ObjReferenceMap &refs);
    bool stripMetadata();

    // Pruning Operations
    // void pruneObject(const std::string &objId);

    // Grafting Operations
    std::set<std::string> getSubtreeObjects(const std::string &treeId);
    std::set<std::string> walkHistory(HistoryCB &cb);
    std::string lookup(const Commit &c, const std::string &path);
    std::string graftSubtree(LocalRepo *r,
                             const std::string &srcPath,
                             const std::string &dstPath);

    // Working Directory Operations
    std::set<std::string> listBranches();
    std::string getBranch();
    void setBranch(const std::string &name);
    std::string getHead();
    void updateHead(const std::string &commitId);
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
    void createObjDirs(const std::string &objId);
public: // Hack to enable rebuild operations
    std::string objIdToPath(const std::string &objId);
private:
    // Variables
    bool opened;
    std::string rootPath;
    std::string id;
    std::string version;
    Index index;
    SnapshotIndex snapshots;
    std::map<std::string, Peer> peers;

    // Remote Operations
    Repo *remoteRepo;

    // Caches
    LRUCache<std::string, ObjectInfo, 128> _objectInfoCache;
    // TODO: ulimit is 256 files open on OSX, need to support 2 repos open at a
    // time?
    LRUCache<std::string, LocalObject::sp, 96> _objectCache;

    // Friends
    friend int LocalRepo_PeerHelper(void *arg, const char *path);
};

#endif
