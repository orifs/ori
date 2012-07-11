#include "ori_fuse.h"

#include <errno.h>

ori_priv::ori_priv(const std::string &repoPath)
    : repo(new LocalRepo(repoPath)),
    head(new Commit()),
    headtree(new Tree())
{
    FUSE_LOG("opening repo at %s", repoPath.c_str());
    if (ori_open_log(repo) < 0) {
        FUSE_LOG("error opening repo log %s", strerror(errno));
        exit(1);
    }

    head->fromBlob(repo->getPayload(repo->getHead()));
    headtree->fromBlob(repo->getPayload(head->getTree()));
}

ori_priv::~ori_priv()
{
    delete headtree;
    delete head;
    delete repo;
}

Tree *
ori_priv::getTree(const std::string &hash)
{
    if (!treeCache.hasKey(hash)) {
        Tree t;
        t.fromBlob(repo->getPayload(hash));
        treeCache.put(hash, t);
    }
    return &treeCache.get(hash);
}

ObjectInfo *
ori_priv::getObjectInfo(const std::string &hash)
{
    if (!objInfoCache.hasKey(hash)) {
        LocalObject lo = repo->getLocalObject(hash);
        objInfoCache.put(hash, lo.getInfo());
    }
    return &objInfoCache.get(hash);
}




ori_priv*
ori_getpriv()
{
    return (ori_priv *)fuse_get_context()->private_data;
}
