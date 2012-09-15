#define FUSE_USE_VERSION 26
#include <fuse.h>

#include <string>
#include <tr1/memory>

#include "tuneables.h"
#include "localrepo.h"
#include "commit.h"
#include "tree.h"
#include "treediff.h"
#include "largeblob.h"
#include "lrucache.h"
#include "rwlock.h"

#define NULL_FH 0
#define ORI_CONTROL_FILEPATH "/" ORI_CONTROL_FILENAME
#define ORI_SNAPSHOT_DIRNAME ".snapshot"
#define ORI_SNAPSHOT_DIRPATH "/" ORI_SNAPSHOT_DIRNAME

// logging.cc
#ifdef DEBUG
#define FUSE_LOG(fmt, ...) ori_fuse_log(fmt "\n", ##__VA_ARGS__)
#else
#define FUSE_LOG(fmt, ...)
#endif

void ori_fuse_log(const char *what, ...);

// parse_opt.cc
struct mount_ori_config {
    char *repo_path;
    char *clone_path;
};

void mount_ori_parse_opt(struct fuse_args *args, mount_ori_config *conf);

// ori_openedfiles.cc
class OpenedFileMgr
{
public:
    OpenedFileMgr();

    void openedFile(const std::string &tempFileName, int fd);
    void closedFile(int fd);
    void closedFile(const std::string &tempFileName);

    void removeUnused();
    bool isOpen(const std::string &tempFileName) const;
    bool anyFilesOpen() const;

    /// Clients responsible for their own locking
    RWLock lock_tempfiles;

private:
    uint64_t numOpenHandles;
    // map from temp filename to number of open handles
    std::map<std::string, uint32_t> openedFiles;
    std::map<int, std::string> fdToFilename;
};


// ori_priv.cc
struct ExtendedTreeEntry
{
    ExtendedTreeEntry() : changedData(false){}
    bool changedData;

    TreeEntry te;
    TreeDiffEntry tde;
};

struct ori_priv
{
    ori_priv(const std::string &repoPath);
    ~ori_priv();

    void _resetHead(const ObjectHash &chash);

    LRUCache<ObjectHash, Tree, 128> treeCache;
    LRUCache<ObjectHash, std::tr1::shared_ptr<LargeBlob>, 64> lbCache;
    LRUCache<ObjectHash, ObjectInfo, 128> objInfoCache;

    LRUCache<std::string, nlink_t, 128> nlinkCache;
    LRUCache<std::string, TreeEntry, FUSE_ENTRY_CACHE_SIZE> teCache;
    LRUCache<std::string, ExtendedTreeEntry, FUSE_ENTRY_CACHE_SIZE> eteCache;

    LocalRepo *repo;
    Commit *head;
    Tree *headtree;

    // Holding temporary written data
    TreeDiff *currTreeDiff;
    TempDir::sp currTempDir;
    
    OpenedFileMgr openedFiles;

    // Lock in this order
    RWLock lock_repo; // repo, head(tree), tempdir
    RWLock lock_cmd_output;

    // Functions to access the cache
    Tree getTree(const ObjectHash &hash, RWKey::sp repoKey);
    LargeBlob getLargeBlob(const ObjectHash &hash);
    ObjectInfo getObjectInfo(const ObjectHash &hash);

    bool getETE(const char *path, ExtendedTreeEntry &ete);
    nlink_t computeNLink(const char *path);

    // Initialize temporary written data
    RWKey::sp startWrite(RWKey::sp repoKey=RWKey::sp());
    bool mergeAndCommit(const TreeDiffEntry &tde, RWKey::sp repoKey);
    // Commit temp data to a FUSE commit
    RWKey::sp fuseCommit(RWKey::sp repoKey=RWKey::sp());
    // Make the last FUSE commit permanent
    RWKey::sp commitPerm();


    // Output buffer for commands (ori_cmdoutput.cc)
    std::string outputBuffer;
    void printf(const char *fmt, ...);
    size_t readOutput(char *buf, size_t n);

private:
    bool getTreeEntry(const char *cpath, TreeEntry &te, RWKey::sp repoKey);
};

ori_priv *ori_getpriv();


// repo_cmd.cc
struct RepoCmd {
    const char *cmd_name;
    void (*func)(ori_priv *);
};

extern RepoCmd _commands[];
