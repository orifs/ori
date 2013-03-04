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

#include <stdint.h>

#include <string>
#include <iostream>
#include <iomanip>
#include <utility>

#include <sys/stat.h>

#include <oriutil/debug.h>
#include <oriutil/oriutil.h>
#include <oriutil/dag.h>
#include <oriutil/objecthash.h>
#include <ori/localrepo.h>

using namespace std;

extern LocalRepo repository;

extern "C" {
#include <libdiffmerge/blob.h>
int blob_merge(Blob *pPivot, Blob *pV1, Blob *pV2, Blob *pOut);
};

int
cmd_merge(int argc, const char *argv[])
{
    if (argc != 2) {
	cout << "merge takes one arguments!" << endl;
	cout << "usage: ori merge <commit 1>" << endl;
	return 1;
    }

    ObjectHash p1 = repository.getHead();
    ObjectHash p2 = ObjectHash::fromHex(argv[1]);

    // Find lowest common ancestor
    DAG<ObjectHash, Commit> cDag = repository.getCommitDag();
    ObjectHash lca;

    lca = cDag.findLCA(p1, p2);
#ifdef DEBUG
    cout << "LCA: " << lca.hex() << endl;
#endif /* DEBUG */

    // Construct tree diffs to ancestor to find conflicts
    TreeDiff td1, td2;

    Commit c1 = repository.getCommit(p1);
    Commit c2 = repository.getCommit(p2);
    Commit cc;

    Tree t1 = repository.getTree(c1.getTree());
    Tree t2 = repository.getTree(c2.getTree());
    Tree tc = Tree();
    
    if (lca != EMPTY_COMMIT) {
	Commit cc = repository.getCommit(lca);
	tc = repository.getTree(cc.getTree());
    }

    td1.diffTwoTrees(t1.flattened(&repository), tc.flattened(&repository));
    td2.diffTwoTrees(t2.flattened(&repository), tc.flattened(&repository));

#ifdef DEBUG
    printf("Tree 1:\n");
    for (size_t i = 0; i < td1.entries.size(); i++) {
        printf("%c   %s\n",
                td1.entries[i].type,
                td1.entries[i].filepath.c_str());
    }
    printf("Tree 2:\n");
    for (size_t i = 0; i < td2.entries.size(); i++) {
        printf("%c   %s\n",
                td2.entries[i].type,
                td2.entries[i].filepath.c_str());
    }
#endif /* DEBUG */

    // Compute merge
    TreeDiff mdiff;
    mdiff.mergeTrees(td1, td2);

#ifdef DEBUG
    printf("Merged Tree:\n");
    for (size_t i = 0; i < mdiff.entries.size(); i++) {
        printf("%c   %s\n",
                mdiff.entries[i].type,
                mdiff.entries[i].filepath.c_str());
    }
#endif /* DEBUG */

    // Setup merge state
    MergeState state;
    state.setParents(p1, p2);

    repository.setMergeState(state);

    // Create diff of working directory updates
    TreeDiff wdiff;
    wdiff.mergeChanges(td1, mdiff);

#ifdef DEBUG
    printf("Merged Tree Diff:\n");
    for (size_t i = 0; i < wdiff.entries.size(); i++) {
        printf("%c   %s\n",
                wdiff.entries[i].type,
                wdiff.entries[i].filepath.c_str());
    }
#endif /* DEBUG */

    printf("Updating working directory\n");
    bool conflictExists = false;
    // Update working directory as necessary
    for (size_t i = 0; i < wdiff.entries.size(); i++) {
	TreeDiffEntry e = wdiff.entries[i];
	string path  = repository.getRootPath() + e.filepath;

	if (e.type == TreeDiffEntry::NewFile) {
	    printf("N       %s\n", e.filepath.c_str());
	    repository.copyObject(e.hashes.first, path);
	} else if (mdiff.entries[i].type == TreeDiffEntry::NewDir) {
	    printf("N       %s\n", e.filepath.c_str());
	    mkdir(path.c_str(), 0755);
	} else if (mdiff.entries[i].type == TreeDiffEntry::DeletedFile) {
	    printf("D       %s\n", e.filepath.c_str());
	    Util_DeleteFile(path);
	} else if (mdiff.entries[i].type == TreeDiffEntry::DeletedDir) {
	    printf("D       %s\n", e.filepath.c_str());
	    rmdir(path.c_str()); 
	} else if (mdiff.entries[i].type == TreeDiffEntry::Modified) {
	    printf("U       %s\n", e.filepath.c_str());
	    repository.copyObject(e.hashes.first, path);
        } else if (mdiff.entries[i].type == TreeDiffEntry::MergeConflict) {
            int status;
            Blob pivot, a, b, out;
            string path;
            string pivotStr;
            string aStr;
            string bStr;
            
            path = repository.getRootPath() + mdiff.entries[i].filepath;
           
            if (mdiff.entries[i].hashBase.first == EMPTYFILE_HASH) {
                pivotStr = "";
            } else {
                pivotStr = repository.getPayload(mdiff.entries[i].hashBase.first);
            }
            aStr = repository.getPayload(mdiff.entries[i].hashA.first);
            bStr = repository.getPayload(mdiff.entries[i].hashB.first);

            blob_init(&pivot, pivotStr.data(), pivotStr.size());
            blob_init(&a, aStr.data(), aStr.size());
            blob_init(&b, bStr.data(), bStr.size());
            blob_zero(&out);
            status = blob_merge(&pivot, &a, &b, &out);
            if (status == 0) {
                printf("U       %s (MERGED)\n", e.filepath.c_str());
                blob_write_to_file(&out, path.c_str());
            } else {
                printf("X       %s (CONFLICT)\n", e.filepath.c_str());
                repository.copyObject(mdiff.entries[i].hashBase.first,
                                      path + ".base");
                repository.copyObject(mdiff.entries[i].hashA.first,
                                      path + ".yours");
                repository.copyObject(mdiff.entries[i].hashB.first,
                                      path + ".theirs");
                conflictExists = true;
                // XXX: Allow optional call to external diff tool
            }
        } else if (mdiff.entries[i].type == TreeDiffEntry::FileDirConflict) {
            NOT_IMPLEMENTED(false);
	} else {
	    printf("Unsupported TreeDiffEntry type %c\n", mdiff.entries[i].type);
	    NOT_IMPLEMENTED(false);
	}
    }

    if (conflictExists) {
        printf("One or more conflicts remain, you need to manually inspect\n");
        printf("the conflicting files with the extentions .base, .yours,\n");
        printf("and .theirs appended to the end.\n");
    } else {
        printf("Conflicts resolved automatically.\n");
        // XXX: Automatically commit if everything is resolved
    }

    return 0;
}

