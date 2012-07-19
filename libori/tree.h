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

class Repo;
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
    std::string hash() const; // TODO: cache this

    typedef std::map<std::string, TreeEntry> Flat;
    Flat flattened(Repo *r) const;
    static Tree unflatten(const Flat &flat, Repo *r);


    std::map<std::string, TreeEntry> tree;
};

#endif /* __TREE_H__ */
