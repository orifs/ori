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
};

void mount_ori_parse_opt(struct fuse_args *args, mount_ori_config *conf);

// ori_priv.cc
struct ori_priv
{
    ori_priv(const std::string &repoPath);
    ~ori_priv();

    // Functions to access the cache
    Tree *getTree(const std::string &hash);
    LargeBlob *getLargeBlob(const std::string &hash);
    ObjectInfo *getObjectInfo(const std::string &hash);

    LRUCache<std::string, Tree, 128> treeCache;
    LRUCache<std::string, std::tr1::shared_ptr<LargeBlob>, 64> lbCache;
    LRUCache<std::string, ObjectInfo, 128> objInfoCache;

    LocalRepo *repo;
    Commit *head;
    Tree *headtree;

    // Holding temporary written data
    TreeDiff *currTreeDiff;
};

ori_priv *ori_getpriv();

