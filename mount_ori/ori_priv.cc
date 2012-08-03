
#include "ori_fuse.h"

#include <string.h>
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

    _resetHead();
}

ori_priv::~ori_priv()
{
    delete headtree;
    delete head;
    delete repo;
}

void
ori_priv::_resetHead()
{
    if (repo->getHead() != EMPTY_COMMIT) {
        *head = Commit();
        head->fromBlob(repo->getPayload(repo->getHead()));
        *headtree = Tree();
        headtree->fromBlob(repo->getPayload(head->getTree()));
    }
}

Tree *
ori_priv::getTree(const ObjectHash &hash)
{
    if (!treeCache.hasKey(hash)) {
        Tree t;
        t.fromBlob(repo->getPayload(hash));
        treeCache.put(hash, t);
    }
    return &treeCache.get(hash);
}

LargeBlob *
ori_priv::getLargeBlob(const ObjectHash &hash)
{
    if (!lbCache.hasKey(hash)) {
        std::tr1::shared_ptr<LargeBlob> lb(new LargeBlob(repo));
        lb->fromBlob(repo->getPayload(hash));
        lbCache.put(hash, lb);
    }
    return lbCache.get(hash).get();
}

ObjectInfo *
ori_priv::getObjectInfo(const ObjectHash &hash)
{
    if (!objInfoCache.hasKey(hash)) {
        ObjectInfo info = repo->getObjectInfo(hash);
        objInfoCache.put(hash, info);
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

bool
ori_priv::merge(const TreeDiffEntry &tde)
{
    assert(currTreeDiff != NULL);
    eteCache.invalidate(tde.filepath);
    return currTreeDiff->merge(tde);
}

void
ori_priv::commitWrite()
{
    if (currTreeDiff != NULL) {
        FUSE_LOG("committing");

        ObjectHash tip = repo->getHead();

        Tree tip_tree;
        if (tip != EMPTY_COMMIT) {
            Commit c = repo->getCommit(tip);
            tip_tree = repo->getTree(c.getTree());
        }
        Tree new_tree = currTreeDiff->applyTo(tip_tree.flattened(repo),
                currTempDir.get());

        Commit newCommit;
        newCommit.setMessage("Commit from FUSE.");
        repo->commitFromObjects(new_tree.hash(), currTempDir.get(),
                newCommit);

        _resetHead();

        delete currTreeDiff;
        currTreeDiff = NULL;
        currTempDir.reset();
        eteCache.clear();
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

