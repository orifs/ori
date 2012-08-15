
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

    _resetHead(NULL);
}

ori_priv::~ori_priv()
{
    delete headtree;
    delete head;
    delete repo;
}

void
ori_priv::_resetHead(Commit *c)
{
    if (c == NULL && repo->getHead() != EMPTY_COMMIT) {
        *head = Commit();
        head->fromBlob(repo->getPayload(repo->getHead()));
    }
    else {
        *head = *c;
    }

    *headtree = Tree();
    if (head->getTree() != EMPTY_COMMIT)
        headtree->fromBlob(repo->getPayload(head->getTree()));
}


Tree *
ori_priv::getTree(const ObjectHash &hash)
{
    RWKey::sp key = lock_cache.readLock();

    if (!treeCache.hasKey(hash)) {
        Tree t;
        t.fromBlob(repo->getPayload(hash));

        key.reset(); key = lock_cache.writeLock();
        treeCache.put(hash, t);
    }
    return &treeCache.get(hash);
}

LargeBlob *
ori_priv::getLargeBlob(const ObjectHash &hash)
{
    RWKey::sp key = lock_cache.readLock();

    if (!lbCache.hasKey(hash)) {
        std::tr1::shared_ptr<LargeBlob> lb(new LargeBlob(repo));
        lb->fromBlob(repo->getPayload(hash));

        key.reset(); key = lock_cache.writeLock();
        lbCache.put(hash, lb);
    }
    return lbCache.get(hash).get();
}

ObjectInfo *
ori_priv::getObjectInfo(const ObjectHash &hash)
{
    RWKey::sp key = lock_cache.readLock();

    if (!objInfoCache.hasKey(hash)) {
        ObjectInfo info = repo->getObjectInfo(hash);

        key.reset(); key = lock_cache.writeLock();
        objInfoCache.put(hash, info);
    }
    return &objInfoCache.get(hash);
}

void
ori_priv::startWrite()
{
    RWKey::sp key = lock_repo.writeLock();

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
ori_priv::fuseCommit()
{
    if (currTreeDiff != NULL) {
        FUSE_LOG("committing");

        Tree new_tree = currTreeDiff->applyTo(headtree->flattened(repo),
                currTempDir.get());

        Commit newCommit;
        newCommit.setMessage("Commit from FUSE.");
        repo->commitFromObjects(new_tree.hash(), currTempDir.get(),
                newCommit, "fuse");

        _resetHead(&newCommit);

        delete currTreeDiff;
        currTreeDiff = NULL;
        currTempDir.reset();
        eteCache.clear();
        teCache.clear();
    }
    else {
        FUSE_LOG("commitWrite: nothing to commit");
    }
}

void
ori_priv::commitPerm()
{
    fuseCommit();
    MdTransaction::sp tr(repo->getMetadata().begin());
    tr->setMeta(head->hash(), "status", "normal");
    tr.reset();

    assert(repo->getMetadata().getMeta(head->hash(), "status") == "normal");

    // Purge other FUSE commits
    repo->purgeFuseCommits();
    repo->updateHead(head->hash());

    FUSE_LOG("committing changes permanently");
}




ori_priv*
ori_getpriv()
{
    return (ori_priv *)fuse_get_context()->private_data;
}

