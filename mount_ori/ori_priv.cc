
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
        RWKey::sp repoKey = lock_repo.readLock();
        Tree t;
        t.fromBlob(repo->getPayload(hash));
        repoKey.reset();

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
        RWKey::sp repoKey = lock_repo.readLock();
        std::tr1::shared_ptr<LargeBlob> lb(new LargeBlob(repo));
        lb->fromBlob(repo->getPayload(hash));
        repoKey.reset();

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
        RWKey::sp repoKey = lock_repo.readLock();
        ObjectInfo info = repo->getObjectInfo(hash);
        repoKey.reset();

        key.reset(); key = lock_cache.writeLock();
        objInfoCache.put(hash, info);
    }
    return &objInfoCache.get(hash);
}



bool
ori_priv::getTreeEntry(const char *cpath, TreeEntry &te)
{
    std::string path(cpath);
    if (teCache.hasKey(path)) {
        te = teCache.get(path);
        return true;
    }

    te = repo->lookupTreeEntry(*head, path);
    if (te.type == TreeEntry::Null)
        return false;
    teCache.put(path, te);
    te = teCache.get(path);
    return true;
}

bool
ori_priv::getETE(const char *path, ExtendedTreeEntry &ete)
{
    RWKey::sp cacheKey = lock_cache.readLock();
    if (eteCache.hasKey(path)) {
        ete = eteCache.get(path);
        return true;
    }
    cacheKey.reset(); cacheKey = lock_cache.writeLock();
    RWKey::sp repoKey = lock_repo.readLock();

    ete = ExtendedTreeEntry();
    bool hasTE = getTreeEntry(path, ete.te);
    TreeDiffEntry *tde = NULL;
    if (currTreeDiff != NULL) {
        tde = currTreeDiff->getLatestEntry(path);
    }

    if (!hasTE && tde == NULL) {
        return false;
    }
    else if (tde != NULL && (tde->type == TreeDiffEntry::DeletedFile ||
                             tde->type == TreeDiffEntry::DeletedDir)) {
        return false;
    }
    else if (tde != NULL && tde->type == TreeDiffEntry::Renamed) {
        NOT_IMPLEMENTED(false);
    }

    if (tde != NULL) {
        if (tde->newFilename != "")
            ete.changedData = true;
        ete.tde = *tde;
        if (tde->type == TreeDiffEntry::NewDir)
            ete.te.type = TreeEntry::Tree;
        ete.te.attrs.mergeFrom(tde->newAttrs);
    }

    if (!ete.te.hasBasicAttrs()) {
        FUSE_LOG("TreeEntry missing attrs!");
        return NULL;
    }

    eteCache.put(path, ete);
    return true;
}



RWKey::sp
ori_priv::startWrite()
{
    RWKey::sp key = lock_repo.writeLock();

    if (currTreeDiff == NULL) {
        currTreeDiff = new TreeDiff();
        currTempDir = repo->newTempDir();
        //checkedOutFiles.clear();
    }

    return key;
}

bool
ori_priv::mergeAndCommit(const TreeDiffEntry &tde)
{
    RWKey::sp cacheKey = lock_cache.writeLock();
    RWKey::sp repoKey = lock_repo.writeLock();

    assert(currTreeDiff != NULL);
    eteCache.invalidate(tde.filepath);
    bool needs_commit = currTreeDiff->merge(tde);
    if (needs_commit) {
        fuseCommit(cacheKey, repoKey);
        return true;
    }

    return false;
}

RWKey::sp
ori_priv::fuseCommit(RWKey::sp cacheKey, RWKey::sp repoKey)
{
    if (!cacheKey)
        cacheKey = lock_cache.writeLock();
    if (!repoKey)
        repoKey = lock_repo.writeLock();

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

    return repoKey;
}

void
ori_priv::commitPerm()
{
    RWKey::sp key(fuseCommit());

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

