#include "treediff.h"
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
        sd->td->entries.push_back(diffEntry);
        return 0;
    }

    // Potentially modified file/dir
    const TreeEntry &te = (*it).second;
    if (Util_IsDirectory(fullPath)) {
        if (te.type != TreeEntry::Tree) {
            // File replaced by dir
            diffEntry.type = TreeDiffEntry::Deleted;
            sd->td->entries.push_back(diffEntry);
            diffEntry.type = TreeDiffEntry::NewDir;
            sd->td->entries.push_back(diffEntry);
        }
        return 0;
    }

    if (te.type == TreeEntry::Tree) {
        // Dir replaced by file
        diffEntry.type = TreeDiffEntry::Deleted;
        sd->td->entries.push_back(diffEntry);
        diffEntry.type = TreeDiffEntry::NewFile;
        diffEntry.newFilename = fullPath;
        sd->td->entries.push_back(diffEntry);
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
        sd->td->entries.push_back(diffEntry);
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
            tde.type = TreeDiffEntry::Deleted;
            entries.push_back(tde);
        }
    }
}

