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

#ifndef __TREE_H__
#define __TREE_H__

#include <stdint.h>

#include <string>
#include <map>
#include <vector>

class TreeEntry
{
public:
    TreeEntry();
    ~TreeEntry();
    enum EntryType {
	Null,
	Blob,
        LargeBlob,
	Tree
    };
    EntryType	type;
    uint16_t	mode;
    std::string hash;
    std::string largeHash;
};

class Tree
{
public:
    Tree();
    ~Tree();
    void addObject(const char *path,
                   const std::string &objId,
                   const std::string &lgObjId = "");
    const std::string getBlob() const;
    void fromBlob(const std::string &blob);
    std::map<std::string, TreeEntry> tree;
};

struct TreeDiffEntry
{
    enum DiffType {
        NewFile = 'N',
        NewDir = 'p',
        Deleted = 'D',
        Modified = 'M',
        ModifiedDiff = 'm'
    } type;

    std::string filepath;
    // TODO: uint16_t newmode;
    std::string modifications;
};

class Repo;
class TreeDiff
{
public:
    void diffTwoTrees(const Tree &t1, const Tree &t2);
    void diffToWD(Tree src, Repo *r);

    std::vector<TreeDiffEntry> entries;
};

#endif /* __TREE_H__ */

