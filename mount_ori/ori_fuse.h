#define FUSE_USE_VERSION 26
#include <fuse.h>

#include <string>
#include <tr1/memory>

#include "localrepo.h"
#include "commit.h"
#include "tree.h"
#include "treediff.h"
#include "largeblob.h"
#include "lrucache.h"

// logging.cc
#define FUSE_LOG(fmt, ...) ori_fuse_log(fmt "\n", ##__VA_ARGS__)
void ori_fuse_log(const char *what, ...);

// parse_opt.cc
struct mount_ori_config {
    char *repo_path;
    char *clone_path;
};

void mount_ori_parse_opt(struct fuse_args *args, mount_ori_config *conf);

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

    void _resetHead();

    // Functions to access the cache
    Tree *getTree(const ObjectHash &hash);
    LargeBlob *getLargeBlob(const ObjectHash &hash);
    ObjectInfo *getObjectInfo(const ObjectHash &hash);

    LRUCache<ObjectHash, Tree, 128> treeCache;
    LRUCache<ObjectHash, std::tr1::shared_ptr<LargeBlob>, 64> lbCache;
    LRUCache<ObjectHash, ObjectInfo, 128> objInfoCache;

    LRUCache<std::string, TreeEntry, 128> teCache;
    LRUCache<std::string, ExtendedTreeEntry, 128> eteCache;

    LocalRepo *repo;
    Commit *head;
    Tree *headtree;

    // Holding temporary written data
    TreeDiff *currTreeDiff;
    TempDir::sp currTempDir;
    //std::map<std::string, std::string> checkedOutFiles;

    // Initialize temporary written data
    void startWrite();
    bool merge(const TreeDiffEntry &tde);
    // Commit temp data to repo
    void commitWrite();


    // Output buffer for commands (ori_cmdoutput.cc)
    std::string outputBuffer;
    void printf(const char *fmt, ...);
    size_t readOutput(char *buf, size_t n);
};

ori_priv *ori_getpriv();


// repo_cmd.cc
struct RepoCmd {
    const char *cmd_name;
    void (*func)(ori_priv *);
};

extern RepoCmd _commands[];
