#ifndef __TREEDIFF_H__
#define __TREEDIFF_H__

#include <string>
#include <vector>
#include <tr1/unordered_map>

#include "repo.h"
#include "tree.h"

struct TreeDiffEntry
{
    enum DiffType {
        Noop, // only used as placeholder
        NewFile = 'A',
        NewDir = 'n',
        DeletedFile = 'D',
        DeletedDir = 'd',
        Modified = 'm',
        ModifiedDiff = 'M'
    } type;

    std::string filepath; // path relative to repo, with leading '/'
    // TODO: uint16_t newmode;
    std::string diff;
    std::string newFilename; // filename of a file containing the new contents
};

class TreeDiff
{
public:
    TreeDiff();
    void diffTwoTrees(const Tree &t1, const Tree &t2);
    void diffToDir(Tree src, const std::string &dir, Repo *r);
    TreeDiffEntry *getLatestEntry(const std::string &path);
    void append(const TreeDiffEntry &to_append);
    /** @returns true if merge causes TreeDiff to grow a layer
      * e.g. D+n or d+A */
    bool merge(const TreeDiffEntry &to_merge);

    Tree applyTo(Tree::Flat flat, Repo *dest_repo);

    std::vector<TreeDiffEntry> entries;

private:
    std::tr1::unordered_map<std::string, size_t> latestEntries;
    void _resetLatestEntry(const std::string &filepath);
};

#endif
