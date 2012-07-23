#include "ori_fuse.h"

#include <errno.h>

ori_priv::ori_priv(const std::string &repoPath)
    : repo(new LocalRepo(repoPath)),
    head(new Commit()),
    headtree(new Tree())
{
    FUSE_LOG("opening repo at %s", repoPath.c_str());
    if (!repo->open(repoPath)) {
        FUSE_LOG("error opening repo");
        exit(1);
    }

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

LargeBlob *
ori_priv::getLargeBlob(const std::string &hash)
{
    if (!lbCache.hasKey(hash)) {
        std::tr1::shared_ptr<LargeBlob> lb(new LargeBlob(repo));
        lb->fromBlob(repo->getPayload(hash));
        lbCache.put(hash, lb);
    }
    return lbCache.get(hash).get();
}

ObjectInfo *
ori_priv::getObjectInfo(const std::string &hash)
{
    if (!objInfoCache.hasKey(hash)) {
        LocalObject::sp lo = repo->getLocalObject(hash);
        objInfoCache.put(hash, lo->getInfo());
    }
    return &objInfoCache.get(hash);
}

void
ori_priv::startWrite()
{
    if (currTreeDiff == NULL) {
        currTreeDiff = new TreeDiff();
        currTempDir = repo->newTempDir();
        //checkedOutFiles.clear();
    }
}

void
ori_priv::commitWrite()
{
    if (currTreeDiff != NULL) {
        // TODO
        FUSE_LOG("committing");

        delete currTreeDiff;
        currTempDir.reset();
    }
    else {
        FUSE_LOG("commitWrite: nothing to commit");
    }
}




ori_priv*
ori_getpriv()
{
    return (ori_priv *)fuse_get_context()->private_data;
}

