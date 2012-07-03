#ifndef __LOCALREPO_H__
#define __LOCALREPO_H__

#include "repo.h"

class HistoryCB
{
public:
    virtual ~HistoryCB() { };
    virtual std::string cb(const std::string &commitId, Commit *c) = 0;
};

class LocalRepo : public Repo
{
public:
    LocalRepo(const std::string &root = "");
    ~LocalRepo();
    bool open(const std::string &root = "");
    void close();
    void save();
    // Object Operations
    int addObjectRaw(const ObjectInfo &info,
            const std::string &raw_data);
    int addObject(const ObjectInfo &info, const std::string
            &payload);
    bool hasObject(const std::string &objId);
    Object *getObject(const std::string &objId);

    LocalObject getLocalObject(const std::string &objId);

    std::string addSmallFile(const std::string &path);
    std::pair<std::string, std::string>
        addLargeFile(const std::string &path);
    std::pair<std::string, std::string>
        addFile(const std::string &path);
    virtual std::string addTree(const Tree &tree);
    virtual std::string addCommit(/* const */ Commit &commit);
    //std::string addBlob(const std::string &blob, Object::Type type);
    std::string getPayload(const std::string &objId);
    std::string verifyObject(const std::string &objId);
    bool purgeObject(const std::string &objId);
    size_t sendObject(const char *objId);
    bool copyObject(const std::string &objId, const std::string &path);
    std::set<std::string> listObjects();
    Commit getCommit(const std::string &commitId);
    // Reference Counting Operations
    std::map<std::string, Object::BRState> getRefs(const std::string &objId);
    std::map<std::string, std::map<std::string, Object::BRState> >
        getRefCounts();
    std::map<std::string, std::set<std::string> > computeRefCounts();
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
    std::string getHead();
    void updateHead(const std::string &commitId);
    // General Operations
    std::string getRootPath();
    std::string getLogPath();
    std::string getTmpFile();
    std::string getUUID();
    std::string getVersion();
    // Static Operations
    static std::string findRootPath(const std::string &path = "");
private:
    // Helper Functions
    void createObjDirs(const std::string &objId);
public: // Hack to enable rebuild operations
    std::string objIdToPath(const std::string &objId);
private:
    // Variables
    std::string rootPath;
    std::string id;
    std::string version;
};

#endif
