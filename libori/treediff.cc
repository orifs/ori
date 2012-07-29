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

#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "treediff.h"
#include "debug.h"
#include "util.h"
#include "scan.h"
#include "largeblob.h"

using namespace std;

/*
 * TreeDiff
 */
TreeDiff::TreeDiff()
{
}

void
TreeDiff::diffTwoTrees(const Tree::Flat &t1, const Tree::Flat &t2)
{
    map<string, TreeEntry>::const_iterator it;

    for (it = t1.begin(); it != t1.end(); it++) {
	string path = (*it).first;
	TreeEntry entry = (*it).second;
	map<string, TreeEntry>::const_iterator it2;

	it2 = t2.find(path);
	if (it2 == t2.end()) {
	    TreeDiffEntry diffEntry;

	    // New file or directory
	    diffEntry.filepath = path;
	    if (entry.type == TreeEntry::Tree) {
		diffEntry.type = TreeDiffEntry::NewDir;
	    } else {
		diffEntry.type = TreeDiffEntry::NewFile;
		// diffEntry.newFilename = ...
	    }

	    entries.push_back(diffEntry);
	} else {
	    TreeEntry entry2 = (*it2).second;

	    if (entry.type != TreeEntry::Tree && entry2.type == TreeEntry::Tree) {
		// Replaced file with directory
		TreeDiffEntry diffEntry;

		diffEntry.filepath = path;
		diffEntry.type = TreeDiffEntry::DeletedDir;
		entries.push_back(diffEntry);

		diffEntry.type = TreeDiffEntry::NewFile;
		entries.push_back(diffEntry);
		// diffEntry.newFilename = ...
	    } else if (entry.type == TreeEntry::Tree &&
		       entry2.type != TreeEntry::Tree) {
		// Replaced directory with file
		TreeDiffEntry diffEntry;

		diffEntry.filepath = path;
		diffEntry.type = TreeDiffEntry::DeletedFile;
		entries.push_back(diffEntry);

		diffEntry.type = TreeDiffEntry::NewDir;
		entries.push_back(diffEntry);
	    } else {
		// Check for mismatch
		TreeEntry entry2 = (*it2).second;

		// XXX: This should do the right thing even if for some reason
		// the file was a small file and became a large file.  That 
		// should never happen though!
		if (entry.type != TreeEntry::Tree && entry.hash != entry2.hash) {
		    TreeDiffEntry diffEntry;

		    diffEntry.filepath = path;
		    diffEntry.type = TreeDiffEntry::Modified;
		    // diffEntry.newFilename = ...

		    entries.push_back(diffEntry);
		}
	    }
	}
    }

    for (it = t2.begin(); it != t2.end(); it++) {
	string path = (*it).first;
	TreeEntry entry = (*it).second;
	map<string, TreeEntry>::const_iterator it1;

	it1 = t1.find(path);
	if (it1 == t1.end()) {
	    TreeDiffEntry diffEntry;

	    // Deleted file or directory
	    diffEntry.filepath = path;
	    diffEntry.type = entry.type == TreeEntry::Tree ?
                TreeDiffEntry::DeletedDir : TreeDiffEntry::DeletedFile;

	    entries.push_back(diffEntry);
	}
    }

    return;
}

struct _scanHelperData {
    set<string> *wd_paths;
    map<string, TreeEntry> *flattened_tree;
    TreeDiff *td;
    Commit *commit;

    size_t cwdLen;
    Repo *repo;
};

int _diffToDirHelper(void *arg, const char *path)
{
    string fullPath = path;
    _scanHelperData *sd = (_scanHelperData *)arg;

    string relPath = fullPath.substr(sd->cwdLen);
    sd->wd_paths->insert(relPath);

    TreeDiffEntry diffEntry;
    diffEntry.filepath = relPath;

    map<string, TreeEntry>::iterator it = sd->flattened_tree->find(relPath);
    if (it == sd->flattened_tree->end()) {
        // New file/dir
        if (Util_IsDirectory(fullPath)) {
            diffEntry.type = TreeDiffEntry::NewDir;
        }
        else {
            diffEntry.type = TreeDiffEntry::NewFile;
            diffEntry.newFilename = fullPath;
        }
        sd->td->append(diffEntry);
        return 0;
    }

    // Potentially modified file/dir
    const TreeEntry &te = (*it).second;
    if (Util_IsDirectory(fullPath)) {
        if (te.type != TreeEntry::Tree) {
            // File replaced by dir
            diffEntry.type = TreeDiffEntry::DeletedFile;
            sd->td->append(diffEntry);
            diffEntry.type = TreeDiffEntry::NewDir;
            sd->td->append(diffEntry);
        }
        return 0;
    }

    if (te.type == TreeEntry::Tree) {
        // Dir replaced by file
        diffEntry.type = TreeDiffEntry::DeletedDir;
        sd->td->append(diffEntry);
        diffEntry.type = TreeDiffEntry::NewFile;
        diffEntry.newFilename = fullPath;
        sd->td->append(diffEntry);
        return 0;
    }

    // Check if file is modified
    
    struct stat st_buf;
    if (lstat(path, &st_buf) < 0) {
        perror("lstat");
        return 0;
    }

    bool modified = false;
    if (te.type == TreeEntry::Blob) {
        ObjectInfo info = sd->repo->getObjectInfo(te.hash);
        if (info.payload_size != (size_t)st_buf.st_size ||
                st_buf.st_mtime >= sd->commit->getTime()) {

            std::string newHash = Util_HashFile(fullPath);
            modified = newHash != te.hash;
        }
    }
    else if (te.type == TreeEntry::LargeBlob) {
        LargeBlob lb(sd->repo);
        Object::sp lbObj(sd->repo->getObject(te.hash));
        lb.fromBlob(lbObj->getPayload());
        if (lb.totalSize() != (size_t)st_buf.st_size ||
                st_buf.st_mtime >= sd->commit->getTime()) {

            std::string newHash = Util_HashFile(fullPath);
            modified = newHash != te.largeHash;
        }
    }

    if (modified) {
        diffEntry.type = TreeDiffEntry::Modified;
        diffEntry.newFilename = fullPath;
        sd->td->append(diffEntry);
        return 0;
    }

    return 0;
}

void
TreeDiff::diffToDir(Commit from, const std::string &dir, Repo *r)
{
    Tree src;
    if (from.getTree() != "")
        src = r->getTree(from.getTree());
    Tree::Flat flattened_tree = src.flattened(r);

    size_t dir_size = dir.size();
    if (dir[dir_size-1] == '/')
        dir_size--;

    set<string> wd_paths;
    _scanHelperData sd = {
        &wd_paths,
        &flattened_tree,
        this,
        &from,
        dir_size,
        r};

    // Find additions and modifications
    Scan_RTraverse(dir.c_str(), &sd, _diffToDirHelper);

    // Find deletions
    for (map<string, TreeEntry>::iterator it = flattened_tree.begin();
            it != flattened_tree.end();
            it++) {
        set<string>::iterator wd_it = wd_paths.find((*it).first);
        if (wd_it == wd_paths.end()) {
            TreeDiffEntry tde;
            tde.filepath = (*it).first;
            tde.type = ((*it).second.type == TreeEntry::Tree) ?
                TreeDiffEntry::DeletedDir :
                TreeDiffEntry::DeletedFile;
            append(tde);
        }
    }
}

TreeDiffEntry *
TreeDiff::getLatestEntry(const std::string &path)
{
    std::tr1::unordered_map<std::string, size_t>::iterator it =
        latestEntries.find(path);
    if (it == latestEntries.end()) {
        return NULL;
    }

    return &entries[(*it).second];
}

void TreeDiff::append(const TreeDiffEntry &to_append)
{
    assert(to_append.filepath != "");
    assert(to_append.type != TreeDiffEntry::Noop);
    entries.push_back(to_append);
    latestEntries[to_append.filepath] = entries.size()-1;
}

bool TreeDiff::merge(const TreeDiffEntry &to_merge)
{
    assert(to_merge.filepath != "");

    // Special case for rename
    if (to_merge.type == TreeDiffEntry::Renamed) {
        TreeDiffEntry *dest_e = getLatestEntry(to_merge.newFilename);
        if (dest_e != NULL && dest_e->type != TreeDiffEntry::DeletedFile &&
                dest_e->type != TreeDiffEntry::DeletedDir) {
            printf("TreeDiff::merge: cannot replace an existing file with Renamed\n");
            NOT_IMPLEMENTED(false);
        }

        append(to_merge);
        return true;
    }

    TreeDiffEntry *e = getLatestEntry(to_merge.filepath);
    if (e == NULL) {
        printf("TreeDiff::merge: appending %s\n", to_merge.filepath.c_str());
        append(to_merge);
        return false;
    }

    if ((e->type == TreeDiffEntry::NewFile ||
         e->type == TreeDiffEntry::Modified) &&
        (to_merge.type == TreeDiffEntry::Modified)
       )
    {
        e->newFilename = to_merge.newFilename;
        return false;
    }
    else if ((e->type == TreeDiffEntry::NewFile &&
              to_merge.type == TreeDiffEntry::DeletedFile) ||
             (e->type == TreeDiffEntry::NewDir &&
              to_merge.type == TreeDiffEntry::DeletedDir))
    {
        e->type = TreeDiffEntry::Noop;
        _resetLatestEntry(e->filepath);
        return false;
    }
    else if (e->type == TreeDiffEntry::Modified &&
             to_merge.type == TreeDiffEntry::DeletedFile)
    {
        e->type = TreeDiffEntry::Noop;
        _resetLatestEntry(e->filepath);
        return false;
    }
    else if ((e->type == TreeDiffEntry::DeletedFile &&
              to_merge.type == TreeDiffEntry::NewDir) ||
             (e->type == TreeDiffEntry::DeletedDir &&
              to_merge.type == TreeDiffEntry::NewFile))
    {
        append(to_merge);
        return true;
    }
    // TODO: deletedfile + newfile--merge or append?
    else
    {
        LOG("%c %c\n", e->type, to_merge.type);
        NOT_IMPLEMENTED(false && "Unknown combination of TreeDiffEntries");
    }
    return false;
}


Tree
TreeDiff::applyTo(Tree::Flat flat, Repo *dest_repo)
{
    for (size_t i = 0; i < entries.size(); i++) {
        const TreeDiffEntry &tde = entries[i];
        if (tde.type == TreeDiffEntry::Noop) continue;

        //printf("Applying %c   %s\n", tde.type, tde.filepath.c_str());
        putc(tde.type, stdout);
        if (tde.type == TreeDiffEntry::NewFile) {
            TreeEntry te;
            pair<string, string> hashes = dest_repo->addFile(tde.newFilename);
            te.hash = hashes.first;
            te.largeHash = hashes.second;
            te.type = (hashes.second != "") ? TreeEntry::LargeBlob :
                TreeEntry::Blob;
            // TODO te.mode

            flat.insert(make_pair(tde.filepath, te));
        }
        else if (tde.type == TreeDiffEntry::NewDir) {
            TreeEntry te;
            te.type = TreeEntry::Tree;
            flat.insert(make_pair(tde.filepath, te));
        }
        else if (tde.type == TreeDiffEntry::DeletedDir) {
            assert(flat[tde.filepath].type == TreeEntry::Tree);
            flat.erase(tde.filepath);
#ifdef DEBUG
            // Check that no files still exist in this directory
            for (Tree::Flat::iterator it = flat.begin();
                    it != flat.end();
                    it++) {
                if (strncmp(tde.filepath.c_str(), (*it).first.c_str(),
                            tde.filepath.size()) == 0) {
                    assert(false);
                }
            }
#endif
        }
        else if (tde.type == TreeDiffEntry::DeletedFile) {
            assert(flat[tde.filepath].type == TreeEntry::Blob ||
                   flat[tde.filepath].type == TreeEntry::LargeBlob);
            flat.erase(tde.filepath);
        }
        else if (tde.type == TreeDiffEntry::Modified) {
            TreeEntry te;
            pair<string, string> hashes = dest_repo->addFile(tde.newFilename);
            te.hash = hashes.first;
            te.largeHash = hashes.second;
            te.type = (hashes.second != "") ? TreeEntry::LargeBlob :
                TreeEntry::Blob;
            // TODO te.mode

            flat[tde.filepath] = te;
        }
        else if (tde.type == TreeDiffEntry::Renamed) {
            assert(flat.find(tde.filepath) != flat.end());
            assert(flat.find(tde.newFilename) == flat.end());

            TreeEntry te = flat[tde.filepath];
            flat.erase(tde.filepath);
            flat[tde.newFilename] = te;
        }
        else {
            assert(false);
        }

        if (i % 80 == 0) putc('\n', stdout);
    }

    /*for (Tree::Flat::iterator it = flat.begin();
            it != flat.end();
            it++) {
        printf("%s %s\n", (*it).first.c_str(), (*it).second.hash.c_str());
    }*/
    putc('\n', stdout);

    Tree rval = Tree::unflatten(flat, dest_repo);
    dest_repo->addBlob(Object::Tree, rval.getBlob());
    return rval;
}


void
TreeDiff::_resetLatestEntry(const std::string &filepath)
{
    latestEntries.erase(filepath);
    for (size_t i = 0; i < entries.size(); i++) {
        const TreeDiffEntry &tde = entries[i];
        if (tde.filepath == filepath && tde.type != TreeDiffEntry::Noop)
        {
            latestEntries[tde.filepath] = i;
        }
    }
}