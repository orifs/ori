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

#include <assert.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include <string>
#include <set>
#include <sstream>
#include <iostream>
#include <algorithm>

#include "tree.h"
#include "util.h"
#include "scan.h"
#include "repo.h"
#include "largeblob.h"
#include "debug.h"

using namespace std;

/********************************************************************
 *
 *
 * AttrMap
 *
 *
 ********************************************************************/

AttrMap::AttrMap()
{
}

void AttrMap::setFromFile(const std::string &filename)
{
    struct stat sb;

    if (stat(filename.c_str(), &sb) < 0) {
	perror("stat failed in Tree::addObject");
	assert(false);
    }

    struct passwd *upw = getpwuid(sb.st_uid);
    struct group *ggr = getgrgid(sb.st_gid);
    setAs<size_t>(ATTR_FILESIZE, sb.st_size);
    setAs<mode_t>(ATTR_PERMS, sb.st_mode & ~S_IFDIR & ~S_IFREG);
    attrs[ATTR_USERNAME] = upw->pw_name;
    attrs[ATTR_GROUPNAME] = ggr->gr_name;
    setAs<time_t>(ATTR_CTIME, sb.st_ctime);
    setAs<time_t>(ATTR_MTIME, sb.st_mtime);
}

void AttrMap::setCreation(mode_t perms)
{
    struct passwd *upw = getpwuid(geteuid());
    struct group *ggr = getgrgid(getegid());
    setAs<size_t>(ATTR_FILESIZE, 0);
    setAs<mode_t>(ATTR_PERMS, perms);
    attrs[ATTR_USERNAME] = upw->pw_name;
    attrs[ATTR_GROUPNAME] = ggr->gr_name;

    time_t currTime = time(NULL);
    setAs<time_t>(ATTR_CTIME, currTime);
    setAs<time_t>(ATTR_MTIME, currTime);
}

void AttrMap::mergeFrom(const AttrMap &other)
{
    for (const_iterator it = other.begin();
            it != other.end();
            it++) {
        attrs[(*it).first] = (*it).second;
    }
}

template <> const char *
AttrMap::getAs<const char *>(const std::string &attrName) {
    assert(attrs.find(attrName) != attrs.end());
    return attrs[attrName].c_str();
}


/********************************************************************
 *
 *
 * TreeEntry
 *
 *
 ********************************************************************/

TreeEntry::TreeEntry()
{
    type = Null;
}

TreeEntry::TreeEntry(const ObjectHash &hash, const ObjectHash &lhash)
    : hash(hash), largeHash(lhash)
{
    if (lhash.isEmpty())
        type = Blob;
    else
        type = LargeBlob;
}

TreeEntry::~TreeEntry()
{
}

void
TreeEntry::extractToFile(const std::string &filename, Repo *src) const
{
    assert(type != Null);
    Object::sp o(src->getObject(hash));
    if (type == Blob) {
        bytestream::ap bs(o->getPayloadStream());
        bs->copyToFile(filename);
    }
    else if (type == LargeBlob) {
        ::LargeBlob lb(src);
        lb.fromBlob(o->getPayload());
        lb.extractFile(filename);
    }
    else {
        NOT_IMPLEMENTED(false);
    }
}

bool
TreeEntry::hasBasicAttrs()
{
    const char *names[] = {ATTR_FILESIZE, ATTR_PERMS, ATTR_USERNAME,
        ATTR_GROUPNAME, ATTR_CTIME, ATTR_MTIME};
    for (size_t i = 0; i < sizeof(names) / sizeof(const char *); i++) {
        LOG("Checking attr %s", names[i]);
        if (attrs.attrs.find(names[i]) == attrs.attrs.end()) return false;
    }
    return true;
}


/********************************************************************
 *
 *
 * Tree
 *
 *
 ********************************************************************/

Tree::Tree()
{
}

Tree::~Tree()
{
}

const string
Tree::getBlob() const
{
    strwstream ss;
    size_t size = tree.size();
    ss.writeInt<uint64_t>(size);

    for (map<string, TreeEntry>::const_iterator it = tree.begin();
            it != tree.end();
            it++) {

        const TreeEntry &te = (*it).second;
        if (te.type == TreeEntry::Tree) {
            ss.write("tree", 4);
        } else if (te.type == TreeEntry::Blob) {
            ss.write("blob", 4);
        } else if (te.type == TreeEntry::LargeBlob) {
            ss.write("lgbl", 4);
        } else {
            assert(false);
        }

        ss.writeHash(te.hash);
        if (te.type == TreeEntry::LargeBlob)
            ss.writeHash(te.largeHash);
        ss.writePStr((*it).first);
        
        size_t asize = te.attrs.attrs.size();
        ss.writeInt<uint32_t>(asize);
        for (AttrMap::const_iterator ait = te.attrs.attrs.begin();
                ait != te.attrs.attrs.end();
                ait++) {
            ss.writePStr((*ait).first);
            ss.writePStr((*ait).second);
        }
    }

    return ss.str();
}

void
Tree::fromBlob(const string &blob)
{
    strstream ss(blob);
    size_t num_entries = ss.readInt<uint64_t>();
    
    for (size_t i = 0; i < num_entries; i++) {
        TreeEntry entry;
        char type[5] = {'\0'};
        ss.readExact((uint8_t*)type, 4);
        if (strcmp(type, "tree") == 0) {
            entry.type = TreeEntry::Tree;
        }
        else if (strcmp(type, "blob") == 0) {
            entry.type = TreeEntry::Blob;
        }
        else if (strcmp(type, "lgbl") == 0) {
            entry.type = TreeEntry::LargeBlob;
        }
        else {
            assert(false);
        }

        ss.readHash(entry.hash);
        if (entry.type == TreeEntry::LargeBlob) {
            ss.readHash(entry.largeHash);
        }
        string path;
        int status = ss.readPStr(path);
        assert(status > 0);

        size_t num_attrs = ss.readInt<uint32_t>();
        for (size_t i_a = 0; i_a < num_attrs; i_a++) {
            string attrName, attrValue;

            status = ss.readPStr(attrName);
            assert(status > 0);
            status = ss.readPStr(attrValue);
            assert(status > 0);

            entry.attrs.attrs[attrName] = attrValue;
        }

        tree[path] = entry;
    }
}

void
_recFlatten(
        const std::string &prefix,
        const Tree *t,
        Tree::Flat *rval,
        Repo *r
        )
{
    for (Tree::Flat::const_iterator it = t->tree.begin();
            it != t->tree.end();
            it++) {
        const TreeEntry &te = (*it).second;
        rval->insert(make_pair(prefix + (*it).first, te));
        if (te.type == TreeEntry::Tree) {
            // Recurse further
            Tree subtree = r->getTree(te.hash);
            _recFlatten(prefix + (*it).first + "/",
                    &subtree, rval, r);
        }
    }
}

Tree::Flat
Tree::flattened(Repo *r) const
{
    Tree::Flat rval;
    _recFlatten("/", this, &rval, r);
    return rval;
}

size_t _num_path_components(const std::string &path)
{
    if (path.size() == 0) return 0;
    size_t cnt = 0;
    for (size_t i = 0; i < path.size(); i++) {
        if (path[i] == '/')
            cnt++;
    }
    return 1 + cnt;
}

bool _tree_gt(const std::string &t1, const std::string &t2)
{
    return _num_path_components(t1) > _num_path_components(t2);
}

Tree
Tree::unflatten(const Flat &flat, Repo *r)
{
    map<string, Tree> trees;
    for (Flat::const_iterator it = flat.begin();
            it != flat.end();
            it++) {
        const TreeEntry &te = (*it).second;
        if (te.type == TreeEntry::Tree) {
            if (trees.find((*it).first) == trees.end()) {
                trees[(*it).first] = Tree();
            }
        }
        else if (te.type == TreeEntry::Blob ||
                te.type == TreeEntry::LargeBlob) {
            string treename = StrUtil_Dirname((*it).first);
            if (trees.find(treename) == trees.end()) {
                trees[treename] = Tree();
            }
            string basename = StrUtil_Basename((*it).first);
            trees[treename].tree[basename] = te;
        }
        else {
            assert(false && "TreeEntry is type Null");
        }
    }

    // Get the tree hashes, update parents, add to Repo
    vector<string> tree_names;
    for (map<string, Tree>::const_iterator it = trees.begin();
            it != trees.end();
            it++) {
        tree_names.push_back((*it).first);
    }
    // Update leaf trees (no child directories) first
    std::sort(tree_names.begin(), tree_names.end(), _tree_gt);

    for (size_t i = 0; i < tree_names.size(); i++) {
        const string &tn = tree_names[i];
        if (tn.size() == 0) continue;
        string blob = trees[tn].getBlob();
        ObjectHash hash = Util_HashString(blob);

        // Add to Repo
        r->addObject(ObjectInfo::Tree, hash, blob);

        // Add to parent
        TreeEntry te = (*flat.find(tree_names[i])).second;
        te.hash = hash;
        te.type = TreeEntry::Tree;
        assert(te.hasBasicAttrs());

        string parent = StrUtil_Dirname(tn);
        trees[parent].tree[StrUtil_Basename(tn)] = te;
    }

    r->addBlob(ObjectInfo::Tree, trees[""].getBlob());

    return trees[""];
}

ObjectHash
Tree::hash() const
{
    return Util_HashString(getBlob());
}
