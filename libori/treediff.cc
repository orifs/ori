#include "treediff.h"
#include "debug.h"
#include "util.h"
#include "scan.h"

using namespace std;

/*
 * TreeDiff
 */
TreeDiff::TreeDiff()
{
}

void
TreeDiff::diffTwoTrees(const Tree &t1, const Tree &t2)
{
}

struct _scanHelperData {
    set<string> *wd_paths;
    map<string, TreeEntry> *flattened_tree;
    TreeDiff *td;

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
    // TODO: heuristic (mtime, filesize) for determining whether a file has been modified
    std::string newHash = Util_HashFile(fullPath);
    bool modified = false;
    if (te.type == TreeEntry::Blob) {
        modified = newHash != te.hash;
    }
    else if (te.type == TreeEntry::LargeBlob) {
        modified = newHash != te.largeHash;
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
TreeDiff::diffToDir(Tree src, const std::string &dir, Repo *r)
{
    set<string> wd_paths;
    Tree::Flat flattened_tree = src.flattened(r);
    /*for (Tree::Flat::iterator it = flattened_tree.begin();
            it != flattened_tree.end();
            it++) {
        printf("%s\n", (*it).first.c_str());
    }*/

    size_t dir_size = dir.size();
    if (dir[dir_size-1] == '/')
        dir_size--;

    _scanHelperData sd = {
        &wd_paths,
        &flattened_tree,
        this,
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

    TreeDiffEntry *e = getLatestEntry(to_merge.filepath);
    if (e == NULL) {
        printf("TreeDiff::merge: appending %s\n", to_merge.filepath.c_str());
        append(to_merge);
        return false;
    }

    // TODO: merge diffs
    if (e->type == TreeDiffEntry::ModifiedDiff || to_merge.type ==
            TreeDiffEntry::ModifiedDiff)
        NOT_IMPLEMENTED(false);

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

        printf("Applying %c   %s\n", tde.type, tde.filepath.c_str());
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
        else if (tde.type == TreeDiffEntry::ModifiedDiff) {
            // TODO: apply diff
            NOT_IMPLEMENTED(false);
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
