#ifndef __TREEDIFF_H__
#define __TREEDIFF_H__

#include <string>
#include <vector>

#include "repo.h"
#include "tree.h"

struct TreeDiffEntry
{
    enum DiffType {
        NewFile = 'A',
        NewDir = 'n',
        Deleted = 'D',
        Modified = 'M',
        ModifiedDiff = 'm'
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

    std::vector<TreeDiffEntry> entries;
};

#endif
