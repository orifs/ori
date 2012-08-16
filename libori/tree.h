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

#include <cassert>
#include <stdint.h>
#include <string.h>

#include <string>
#include <map>
#include <vector>

#include "objecthash.h"

#define ATTR_FILESIZE "Ssize"
#define ATTR_PERMS "Sperms"
#define ATTR_USERNAME "Suser"
#define ATTR_GROUPNAME "Sgroup"
#define ATTR_CTIME "Sctime"
#define ATTR_MTIME "Smtime"
#define ATTR_SYMLINK "Slink"

class AttrMap {
    typedef std::map<std::string, std::string> _MapType;
public:
    AttrMap();

    template <typename T>
    T getAs(const std::string &attrName) {
        assert(attrs.find(attrName) != attrs.end());
        assert(attrs[attrName].size() >= sizeof(T));
        return *(T *)attrs[attrName].data();
    }

    template <typename T>
    void setAs(const std::string &attrName, const T &value) {
        attrs[attrName].resize(sizeof(T));
        memcpy(&attrs[attrName][0], &value, sizeof(T));
    }

    bool has(const std::string &attrName) const;

    void setFromFile(const std::string &filename);
    void setCreation(mode_t perms);
    void mergeFrom(const AttrMap &other);


    _MapType attrs;

    // Mirrorring std::map functions
    typedef _MapType::iterator iterator;
    typedef _MapType::const_iterator const_iterator;
    iterator begin() { return attrs.begin(); }
    const_iterator begin() const { return attrs.begin(); }
    iterator end() { return attrs.end(); }
    const_iterator end() const { return attrs.end(); }

    const_iterator find(const std::string &key) const { return attrs.find(key); }
    void insert(const _MapType::value_type &val) { attrs.insert(val); }
};

class Repo;
class Tree;
class TreeEntry
{
public:
    enum EntryType {
	Null,
	Blob,
        LargeBlob,
	Tree
    };

    TreeEntry();
    TreeEntry(const ObjectHash &hash, const ObjectHash &lhash);
    ~TreeEntry();

    EntryType	type;
    AttrMap attrs;

    ObjectHash hash;
    ObjectHash largeHash;

    // TODO: not sure this is the best place
    void extractToFile(const std::string &filename, Repo *src) const;

    bool hasBasicAttrs();
};

class Tree
{
public:
    Tree();
    ~Tree();
    const std::string getBlob() const;
    void fromBlob(const std::string &blob);
    ObjectHash hash() const; // TODO: cache this

    typedef std::map<std::string, TreeEntry> Flat;
    Flat flattened(Repo *r) const;
    static Tree unflatten(const Flat &flat, Repo *r);


    std::map<std::string, TreeEntry> tree;
};

#endif /* __TREE_H__ */
