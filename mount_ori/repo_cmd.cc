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

#include "logging.h"
#include "openedfilemgr.h"
#include "ori_fuse.h"

void cmd_commit(ori_priv *p)
{
    printf("Executing commit\n");
    RWKey::sp repoKey = p->commitPerm();
    p->repo->sync();
    p->printf("Commit from FUSE.\nCommit Hash: %s\nTree Hash: %s\n",
	   p->head->hash().hex().c_str(),
	   p->headtree->hash().hex().c_str());
}

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

void cmd_tip(ori_priv *p)
{
    p->printf("%s\n", p->head->hash().hex().c_str());
}

RepoCmd _commands[] = {
    { "commit", cmd_commit },
    { "status", cmd_status },
    { "tip", cmd_tip },
    { NULL, NULL }
};

