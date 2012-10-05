/*
 * Copyright (c) 2012 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __ORI_FUSE_H__
#define __ORI_FUSE_H__

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include <string>
#include <tr1/memory>

#include <ori/localrepo.h>
#include <ori/commit.h>
#include <ori/tree.h>
#include <ori/treediff.h>
#include <ori/largeblob.h>
#include <ori/lrucache.h>
#include <ori/rwlock.h>

#include "fuse_tuneables.h"

#define NULL_FH 0
#define ORI_CONTROL_FILENAME ".ori_control"
#define ORI_CONTROL_FILEPATH "/" ORI_CONTROL_FILENAME
#define ORI_SNAPSHOT_DIRNAME ".snapshot"
#define ORI_SNAPSHOT_DIRPATH "/" ORI_SNAPSHOT_DIRNAME

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

    void _resetHead(const ObjectHash &chash);


    LRUCache<ObjectHash, Tree, FUSE_TREE_CACHE_SIZE> treeCache;
    LRUCache<ObjectHash, std::tr1::shared_ptr<LargeBlob>,
	     FUSE_LARGEBLOB_CACHE_SIZE> lbCache;
    LRUCache<ObjectHash, ObjectInfo, FUSE_OBJINFO_CACHE_SIZE> objInfoCache;

    LRUCache<std::string, nlink_t, FUSE_NLINK_CACHE_SIZE> nlinkCache;
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
    // Make the last FUSE commit permanent
    RWKey::sp commitPerm();


    // Output buffer for commands (ori_cmdoutput.cc)
    std::string outputBuffer;
    void printf(const char *fmt, ...);
    size_t readOutput(char *buf, size_t n);

private:
    // Commit temp data to a FUSE commit
    RWKey::sp fuseCommit(RWKey::sp repoKey=RWKey::sp());
    bool getTreeEntry(const char *cpath, TreeEntry &te, RWKey::sp repoKey);
};

ori_priv *ori_getpriv();


// repo_cmd.cc
struct RepoCmd {
    const char *cmd_name;
    void (*func)(ori_priv *);
};

extern RepoCmd _commands[];

#endif /* __ORI_FUSE_H__ */

