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

#include "repo.h"
#include "tree.h"

struct TreeDiffEntry
{
    enum DiffType {
        NewFile = 'A',
        NewDir = 'N',
        Deleted = 'D',
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

    Tree applyTo(Tree::Flat flat, Repo *dest_repo);

    std::vector<TreeDiffEntry> entries;
};

#endif
