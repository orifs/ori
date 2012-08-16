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
#include <pwd.h>
#include <grp.h>

#include "treediff.h"
#include "debug.h"
#include "util.h"
#include "scan.h"
#include "largeblob.h"

using namespace std;

TreeDiffEntry::TreeDiffEntry()
    : type(Noop),
      filepath(""),
      newFilename(""),
      hashes(make_pair(ObjectHash(), ObjectHash()))
{
}

TreeDiffEntry::TreeDiffEntry(const std::string &filepath, DiffType t)
    : type(t),
      filepath(filepath),
      newFilename(""),
      hashes(make_pair(ObjectHash(), ObjectHash()))
{
}

void TreeDiffEntry::_diffAttrs(const AttrMap &a_old, const AttrMap &a_new)
{
    for (AttrMap::const_iterator it = a_new.begin();
            it != a_new.end();
            it++) {
        // TODO: handle deletion of attrs?
        AttrMap::const_iterator old_it = a_old.find((*it).first);
        if (old_it == a_old.end())
            newAttrs.insert((*it));
        if ((*old_it).second != (*it).second)
            newAttrs.insert(*it);
    }
}

/*
 * TreeDiff
 */
TreeDiff::TreeDiff()
{
}

// TODO: 
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
		diffEntry.newFilename = "";
		diffEntry.hashes = make_pair(entry.hash, entry.largeHash);
	    }

	    append(diffEntry);
	} else {
	    TreeEntry entry2 = (*it2).second;

	    if (entry.type != TreeEntry::Tree && entry2.type == TreeEntry::Tree) {
		// Replaced directory with file
		TreeDiffEntry diffEntry;

		diffEntry.filepath = path;
		diffEntry.type = TreeDiffEntry::DeletedDir;
		append(diffEntry);

		diffEntry.type = TreeDiffEntry::NewFile;
		append(diffEntry);
		diffEntry.newFilename = "";
		diffEntry.hashes = make_pair(entry2.hash, entry2.largeHash);
	    } else if (entry.type == TreeEntry::Tree &&
		       entry2.type != TreeEntry::Tree) {
		// Replaced file with directory
		TreeDiffEntry diffEntry;

		diffEntry.filepath = path;
		diffEntry.type = TreeDiffEntry::DeletedFile;
		append(diffEntry);

		diffEntry.type = TreeDiffEntry::NewDir;
		append(diffEntry);
	    } else {
		// Check for mismatch
		TreeEntry entry2 = (*it2).second;

		// XXX: This should do the right thing even if for some reason
		// the file was a small file and became a large file (with the 
		// exact same content).  That should never happen though!
		if (entry.type != TreeEntry::Tree && entry.hash != entry2.hash) {
		    TreeDiffEntry diffEntry;

		    diffEntry.filepath = path;
		    diffEntry.type = TreeDiffEntry::Modified;
		    diffEntry.newFilename = "";
		    diffEntry.hashes = make_pair(entry2.hash, entry2.largeHash);

		    append(diffEntry);
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

	    append(diffEntry);
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
        diffEntry.newAttrs.setFromFile(fullPath);
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
            diffEntry.newAttrs.setFromFile(fullPath);
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
        diffEntry.newAttrs.setFromFile(fullPath);
        sd->td->append(diffEntry);
        return 0;
    }

    // Check if file is modified

    AttrMap newAttrs;
    newAttrs.setFromFile(fullPath);

    bool modified = false;
    if (te.type == TreeEntry::Blob) {
        ObjectInfo info = sd->repo->getObjectInfo(te.hash);
        if (info.payload_size != newAttrs.getAs<size_t>(ATTR_FILESIZE) ||
                newAttrs.getAs<time_t>(ATTR_MTIME) >= sd->commit->getTime()) {

            ObjectHash newHash = Util_HashFile(fullPath);
            modified = newHash != te.hash;
        }
    }
    else if (te.type == TreeEntry::LargeBlob) {
        LargeBlob lb(sd->repo);
        Object::sp lbObj(sd->repo->getObject(te.hash));
        lb.fromBlob(lbObj->getPayload());
        if (lb.totalSize() != newAttrs.getAs<size_t>(ATTR_FILESIZE) ||
                newAttrs.getAs<time_t>(ATTR_MTIME) >= sd->commit->getTime()) {

            ObjectHash newHash = Util_HashFile(fullPath);
            modified = newHash != te.largeHash;
        }
    }

    if (modified) {
        diffEntry.type = TreeDiffEntry::Modified;
        diffEntry.newFilename = fullPath;

        diffEntry._diffAttrs(te.attrs, newAttrs);

        sd->td->append(diffEntry);
        return 0;
    }

    return 0;
}

void
TreeDiff::diffToDir(Commit from, const std::string &dir, Repo *r)
{
    Tree src;
    if (!from.getTree().isEmpty())
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

const TreeDiffEntry *
TreeDiff::getLatestEntry(const std::string &path) const
{
    std::tr1::unordered_map<std::string, size_t>::const_iterator it =
        latestEntries.find(path);
    if (it == latestEntries.end()) {
        return NULL;
    }

    return &entries[(*it).second];
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

/*
 * merges a single entry into the TreeDiff.
 */
bool TreeDiff::mergeInto(const TreeDiffEntry &to_merge)
{
    assert(to_merge.type != TreeDiffEntry::Noop);
    assert(to_merge.filepath != "");

    // Special case for rename
    if (to_merge.type == TreeDiffEntry::Renamed) {
	    const TreeDiffEntry *dest_e = getLatestEntry(to_merge.newFilename);
        if (dest_e != NULL && dest_e->type != TreeDiffEntry::DeletedFile &&
                dest_e->type != TreeDiffEntry::DeletedDir) {
            printf("TreeDiff::mergeInto: cannot replace an existing file with Renamed\n");
            NOT_IMPLEMENTED(false);
        }

        append(to_merge);
        return true;
    }

    TreeDiffEntry *e = getLatestEntry(to_merge.filepath);
    if (e == NULL) {
        printf("TreeDiff::merge: appending %s\n", to_merge.filepath.c_str());
        append(to_merge);
        if (to_merge.type == TreeDiffEntry::NewDir) {
	    // Makes readdir easier to write
            return true;
        }
        return false;
    }

    if ((e->type == TreeDiffEntry::NewFile ||
         e->type == TreeDiffEntry::Modified) &&
        (to_merge.type == TreeDiffEntry::Modified)
       )
    {
        if (e->newFilename == "" || to_merge.newFilename != "")
            e->newFilename = to_merge.newFilename;
        e->newAttrs.mergeFrom(to_merge.newAttrs);
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
        e->type = TreeDiffEntry::DeletedFile;
        return false;
    }
    else if ((e->type == TreeDiffEntry::DeletedFile &&
              to_merge.type == TreeDiffEntry::NewDir) ||
             (e->type == TreeDiffEntry::DeletedDir &&
              to_merge.type == TreeDiffEntry::NewFile))
    {
        // file -> dir or v.v.
        append(to_merge);
        return true;
    }
    else if ((e->type == TreeDiffEntry::DeletedFile &&
              to_merge.type == TreeDiffEntry::NewFile) ||
             (e->type == TreeDiffEntry::DeletedDir &&
              to_merge.type == TreeDiffEntry::NewDir))
    {
        e->type = TreeDiffEntry::Modified;
        e->newFilename = to_merge.newFilename;
        e->newAttrs.mergeFrom(to_merge.newAttrs);
        return true;
    }
    else
    {
        LOG("%c %c\n", e->type, to_merge.type);
        NOT_IMPLEMENTED(false && "Unknown combination of TreeDiffEntries");
    }
    return false;
}

void
TreeDiff::mergeTrees(const TreeDiff &d1, const TreeDiff &d2)
{
    vector<TreeDiffEntry>::const_iterator i1;
    vector<TreeDiffEntry>::const_iterator i2;
    /*
     * Resolve directory level conflicts:
     * 
     * T1 | T2 | Action
     * A  | A  | Conflict if hashes do not match
     * M  | M  | Conflict if hashes do not match
     * D  | D  | Delete file
     * M  | D  | Delete file
     * -  | A  | Add file
     * -  | M  | Merge new modifications
     * -  | D  | Delete file
     *
     * XXX: Missing functionality to transform delete/add as a file move.
     */

    for (i1 = d1.entries.begin(); i1 != d1.entries.end(); i1++)
    {
	bool fdReplace = false;
	vector<TreeDiffEntry>::const_iterator e = i1;
	vector<TreeDiffEntry>::const_iterator e_first;

	/*
	 * The entries array may contain things in any of the following orders.
	 * A/n     - Add file or directory
	 * D/d     - Delete file or directory
	 * D/d n/A - Replace file or directory with a directory or file
	 * m       - Modified
	 *
	 * The 'e' iterator will point to the latest entry and the 'e_first'
	 * will point to the delete if it is a file/directory replace.
	 */

	// Check for file/directory replace
	if (i1->type == TreeDiffEntry::DeletedFile) {
	    e = i1 + 1;
	    e_first = i1;
	    if (e != d1.entries.end() &&
		e->type == TreeDiffEntry::NewDir &&
		e->filepath == e_first->filepath) {
		fdReplace = true;
		i1++;
	    } else {
		e = i1;
	    }
	} else if ((*i1).type == TreeDiffEntry::DeletedDir) {
	    e = i1 + 1;
	    e_first = i1;
	    if (e != d1.entries.end() &&
		e->type == TreeDiffEntry::NewFile &&
		e->filepath == e_first->filepath) {
		fdReplace = true;
		i1++;
	    } else {
		e = i1;
	    }
	}

	// Look in other diff
	const TreeDiffEntry *e_second = d2.getLatestEntry(e->filepath);

	// Determine action
	TreeDiffEntry::DiffType t1, t2;
	t1 = e->type;
	if (e_second) {
	    t2 = e_second->type;
	} else {
	    t2 = TreeDiffEntry::Noop;
	}
	printf("%c %c\n", t1, t2);

	if (t2 == TreeDiffEntry::Noop) {
	    if (fdReplace)
		append(*e_first);
	    append(*e);
	} else if (t1 == TreeDiffEntry::NewFile) {
	    if (t2 == TreeDiffEntry::NewFile) {
		// Conflict?
		if (e->hashes == e_second->hashes) {
		    append(*e);
		} else {
		    // XXX: Merge conflict
		    assert(false);
		}
	    } else if (t2 == TreeDiffEntry::NewDir) {
		// XXX: Merge conflict
		assert(false);
	    }
	} else if (t1 == TreeDiffEntry::NewDir) {
	    if (t2 == TreeDiffEntry::NewDir ||
		(fdReplace && t2 == TreeDiffEntry::Modified)) {
		append(*e);
	    } else if (t2 == TreeDiffEntry::NewFile) {
		// XXX: MergeConflict
		assert(false);
	    }
	} else if (t1 == TreeDiffEntry::DeletedFile) {
	    if (t2 == TreeDiffEntry::NewDir) {
		// Create new dir after file delete
		// XXX: assert file was deleted in t2
		append(*e);
		append(*e_second);
	    } else {
		append(*e);
	    }
	} else if (t1 == TreeDiffEntry::DeletedDir) {
	    if (t2 == TreeDiffEntry::NewFile) {
		// Create new file after directory delete
		// XXX: assert directory was deleted in t2
		append(*e);
		append(*e_second);
	    } else {
		// Append deleted directory
		append(*e);
	    }
	} else if (t1 == TreeDiffEntry::Modified) {
	    if (t2 == TreeDiffEntry::DeletedFile) {
		// Append delete
		append(*e_second);
	    } else if (t2 == TreeDiffEntry::NewDir) {
		// Append delete
		// Append newDir
	    } else if (t2 == TreeDiffEntry::Modified) {
		// Append conflict
	    }
	} else {
	    assert(false);
	}
    }

    // Append any new objects from 't2'
}

Tree
TreeDiff::applyTo(Tree::Flat flat, Repo *dest_repo)
{
    for (size_t i = 0; i < entries.size(); i++) {
        const TreeDiffEntry &tde = entries[i];
        if (tde.type == TreeDiffEntry::Noop) continue;

        printf("Applying %c   %s (%s)\n", tde.type, tde.filepath.c_str(),
                tde.newFilename.c_str());
        //if (i % 80 == 0 && i > 0) putc('\n', stdout);
        //putc(tde.type, stdout);
        if (tde.type == TreeDiffEntry::NewFile) {
            pair<ObjectHash, ObjectHash> hashes;
	    if (tde.newFilename == "") {
		hashes = tde.hashes;
	    } else {
		hashes = dest_repo->addFile(tde.newFilename);
	    }
            TreeEntry te(hashes.first, hashes.second);
            te.attrs.mergeFrom(tde.newAttrs);
            assert(te.hasBasicAttrs());
            flat.insert(make_pair(tde.filepath, te));
        }
        else if (tde.type == TreeDiffEntry::NewDir) {
            TreeEntry te;
            te.type = TreeEntry::Tree;
            te.attrs.mergeFrom(tde.newAttrs);
            assert(te.hasBasicAttrs());
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
            TreeEntry te = flat[tde.filepath];
            if (tde.newFilename != "") {
                pair<ObjectHash, ObjectHash> hashes = dest_repo->addFile(tde.newFilename);
                te.hash = hashes.first;
                te.largeHash = hashes.second;
                te.type = (!hashes.second.isEmpty()) ? TreeEntry::LargeBlob :
                    TreeEntry::Blob;
            } else if (!tde.hashes.first.isEmpty()) {
                pair<ObjectHash, ObjectHash> hashes = tde.hashes;
		te.hash = hashes.first;
		te.largeHash = hashes.second;
                te.type = (!hashes.second.isEmpty()) ? TreeEntry::LargeBlob :
                    TreeEntry::Blob;
	    } else {
                LOG("attribute-only diff");
            }
            
            te.attrs.mergeFrom(tde.newAttrs);
            assert(te.hasBasicAttrs());

            flat[tde.filepath] = te;
        }
        else if (tde.type == TreeDiffEntry::Renamed) {
            assert(flat.find(tde.filepath) != flat.end());
            assert(flat.find(tde.newFilename) == flat.end());

            TreeEntry te = flat[tde.filepath];
            flat.erase(tde.filepath);
            te.attrs.mergeFrom(tde.newAttrs);
            flat[tde.newFilename] = te;
            assert(te.hasBasicAttrs());
        }
        else {
            assert(false);
        }
    }

    /*for (Tree::Flat::iterator it = flat.begin();
            it != flat.end();
            it++) {
        printf("%s %s\n", (*it).first.c_str(), (*it).second.hash.c_str());
    }*/
    putc('\n', stdout);

    Tree rval = Tree::unflatten(flat, dest_repo);
    //dest_repo->addBlob(Object::Tree, rval.getBlob());
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
