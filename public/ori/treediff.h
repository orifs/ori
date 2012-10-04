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

#ifndef __TREEDIFF_H__
#define __TREEDIFF_H__

#include <string>
#include <vector>
#include <tr1/unordered_map>

#include "objecthash.h"
#include "repo.h"
#include "tree.h"

struct TreeDiffEntry
{
    enum DiffType {
        Noop = '-', // only used as placeholder
        NewFile = 'A',
        NewDir = 'n',
        DeletedFile = 'D',
        DeletedDir = 'd',
        Modified = 'm',
        Renamed = 'R',
	// ModifiedDiff = 'M'
	MergeConflict = 'C',
	MergeResolved = 'c',
    } type;

    TreeDiffEntry();
    TreeDiffEntry(const std::string &filepath, DiffType t);

    std::string filepath; // path relative to repo, with leading '/'
    std::string newFilename; // filename of a file containing the new contents
    std::pair<ObjectHash, ObjectHash> hashes;
    AttrMap newAttrs;

    void _diffAttrs(const AttrMap &a_old, const AttrMap &a_new);
};

class TreeDiff
{
public:
    TreeDiff();
    void diffTwoTrees(const Tree::Flat &t1, const Tree::Flat &t2);
    void diffToDir(Commit from, const std::string &dir, Repo *r);
    TreeDiffEntry *getLatestEntry(const std::string &path);
    const TreeDiffEntry *getLatestEntry(const std::string &path) const;
    void append(const TreeDiffEntry &to_append);
    /**
     * @returns true if mergeInto causes TreeDiff to grow a layer
     * e.g. D+n or d+A
     */
    bool mergeInto(const TreeDiffEntry &to_merge);

    void mergeTrees(const TreeDiff &d1, const TreeDiff &d2);
    void mergeChanges(const TreeDiff &d1, const TreeDiff &diff);

    Tree applyTo(Tree::Flat flat, Repo *dest_repo);

    std::vector<TreeDiffEntry> entries;

private:
    std::tr1::unordered_map<std::string, size_t> latestEntries;
    void _resetLatestEntry(const std::string &filepath);
};

#endif
