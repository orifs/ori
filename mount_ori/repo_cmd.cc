#include "ori_fuse.h"

void cmd_status(ori_priv *p)
{
    printf("Executing status\n");
    if (p->currTreeDiff != NULL) {
        p->printf("Changes since last commit:\n");
        for (size_t i = 0; i < p->currTreeDiff->entries.size(); i++) {
            const TreeDiffEntry &tde = p->currTreeDiff->entries[i];
            p->printf("%c   %s\n", tde.type, tde.filepath.c_str());
        }
    }
}

void cmd_commit(ori_priv *p)
{
    printf("Executing commit\n");
    RWKey::sp repoKey = p->commitPerm();
    p->repo->sync();
    p->printf("Commit from FUSE.\nCommit Hash: %s\nTree Hash: %s\n",
	   p->head->hash().hex().c_str(),
	   p->headtree->hash().hex().c_str());
}

RepoCmd _commands[] = {
    { "status", cmd_status },
    { "commit", cmd_commit },
    { NULL, NULL }
};

